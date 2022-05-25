// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "llvm_util/llvm2alive.h"
#include "llvm_util/known_fns.h"
#include "llvm_util/utils.h"
#include "util/sort.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/MemoryBuiltins.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/GetElementPtrTypeIterator.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/IR/Operator.h"
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

using namespace llvm_util;
using namespace IR;
using namespace util;
using namespace std;
using llvm::cast, llvm::dyn_cast, llvm::isa;
using llvm::LLVMContext;

namespace {

FpRoundingMode parse_rounding(llvm::Instruction &i) {
  auto *fp = dyn_cast<llvm::ConstrainedFPIntrinsic>(&i);
  if (!fp || !fp->getRoundingMode().hasValue())
    return {};
  switch (fp->getRoundingMode().getValue()) {
  case llvm::RoundingMode::Dynamic:           return FpRoundingMode::Dynamic;
  case llvm::RoundingMode::NearestTiesToAway: return FpRoundingMode::RNA;
  case llvm::RoundingMode::NearestTiesToEven: return FpRoundingMode::RNE;
  case llvm::RoundingMode::TowardNegative:    return FpRoundingMode::RTN;
  case llvm::RoundingMode::TowardPositive:    return FpRoundingMode::RTP;
  case llvm::RoundingMode::TowardZero:        return FpRoundingMode::RTZ;
  default: UNREACHABLE();
  }
}

FpExceptionMode parse_exceptions(llvm::Instruction &i) {
  auto *fp = dyn_cast<llvm::ConstrainedFPIntrinsic>(&i);
  if (!fp || !fp->getExceptionBehavior().hasValue())
    return {};
  switch (fp->getExceptionBehavior().getValue()) {
  case llvm::fp::ebIgnore:  return FpExceptionMode::Ignore;
  case llvm::fp::ebMayTrap: return FpExceptionMode::MayTrap;
  case llvm::fp::ebStrict:  return FpExceptionMode::Strict;
  default: UNREACHABLE();
  }
}

bool has_constant_expr(llvm::Value *val) {
  return isa<llvm::ConstantExpr, llvm::ConstantAggregate>(val);
}

unsigned constexpr_idx;
unsigned copy_idx;
unsigned alignopbundle_idx;

#if 0
string_view s(llvm::StringRef str) {
  return { str.data(), str.size() };
}
#endif

#define PARSE_UNOP()                       \
  auto ty = llvm_type2alive(i.getType());  \
  auto val = get_operand(i.getOperand(0)); \
  if (!ty || !val)                         \
    return error(i)

#define PARSE_BINOP()                     \
  auto ty = llvm_type2alive(i.getType()); \
  auto a = get_operand(i.getOperand(0));  \
  auto b = get_operand(i.getOperand(1));  \
  if (!ty || !a || !b)                    \
    return error(i)

#define PARSE_TRIOP()                     \
  auto ty = llvm_type2alive(i.getType()); \
  auto a = get_operand(i.getOperand(0));  \
  auto b = get_operand(i.getOperand(1));  \
  auto c = get_operand(i.getOperand(2));  \
  if (!ty || !a || !b || !c)              \
    return error(i)

#define RETURN_IDENTIFIER(op)      \
  do {                             \
    auto ret = op;                 \
    add_identifier(i, *ret.get()); \
    return ret;                    \
  } while (0)



class llvm2alive_ : public llvm::InstVisitor<llvm2alive_, unique_ptr<Instr>> {
  BasicBlock *BB;
  llvm::Function &f;
  const llvm::TargetLibraryInfo &TLI;
  vector<llvm::Instruction*> i_constexprs;
  const vector<string_view> &gvnamesInSrc;
  vector<pair<Phi*, llvm::PHINode*>> todo_phis;
  ostream *out;
  // (LLVM alloca, (Alive2 alloc, has lifetime.start?))
  map<const llvm::AllocaInst *, std::pair<Alloc *, bool>> allocs;


  using RetTy = unique_ptr<Instr>;

  auto DL() const { return f.getParent()->getDataLayout(); }

  template <typename T>
  uint64_t alignment(T &i, llvm::Type *ty) const {
    auto a = i.getAlignment();
    return a != 0 ? a : DL().getABITypeAlignment(ty);
  }

  template <typename T>
  uint64_t pref_alignment(T &i, llvm::Type *ty) const {
    auto a = i.getAlignment();
    return a != 0 ? a : DL().getPrefTypeAlignment(ty);
  }

  Value* convert_constexpr(llvm::ConstantExpr *cexpr) {
    llvm::Instruction *newI = cexpr->getAsInstruction();
    newI->setName("__constexpr_" + to_string(constexpr_idx++));
    i_constexprs.push_back(newI);

    auto ptr = this->visit(*newI);
    if (!ptr)
      return nullptr;

    auto i = ptr.get();
    BB->addInstr(std::move(ptr));
    return i;
  }

  Value* copy_inserter(AggregateValue *ag) {
    auto v = make_unique<UnaryOp>(*const_cast<Type *>(&ag->getType()),
                                  "%__copy_" + to_string(copy_idx++), *ag,
                                  UnaryOp::Copy);
    auto val = v.get();
    BB->addInstr(std::move(v));
    return val;
  }

  auto get_operand(llvm::Value *v) {
    return llvm_util::get_operand(v,
        [this](auto I) { return convert_constexpr(I); },
        [&](auto ag) { return copy_inserter(ag); });
  }

  RetTy NOP(llvm::Instruction &i) {
    // some NOP instruction
    assert(i.getType()->isVoidTy());
    auto true_val = get_operand(llvm::ConstantInt::getTrue(i.getContext()));
    return make_unique<Assume>(*true_val, Assume::AndNonPoison);
  }

public:
  llvm2alive_(llvm::Function &f, const llvm::TargetLibraryInfo &TLI,
              const vector<string_view> &gvnamesInSrc)
      : f(f), TLI(TLI), gvnamesInSrc(gvnamesInSrc), out(&get_outs()) {}

  ~llvm2alive_() {
    for (auto &inst : i_constexprs) {
      remove_value_name(*inst); // otherwise value_names maintain freed pointers
      inst->deleteValue();
    }
  }

  RetTy visitUnaryOperator(llvm::UnaryOperator &i) {
    PARSE_UNOP();
    FpUnaryOp::Op op;
    switch (i.getOpcode()) {
    case llvm::Instruction::FNeg: op = FpUnaryOp::FNeg; break;
    default:
      return error(i);
    }
    RETURN_IDENTIFIER(
      make_unique<FpUnaryOp>(*ty, value_name(i), *val, op, parse_fmath(i)));
  }

  RetTy visitBinaryOperator(llvm::BinaryOperator &i) {
    PARSE_BINOP();
    BinOp::Op alive_op;
    FpBinOp::Op fp_op;
    bool is_fp = false;
    switch (i.getOpcode()) {
    case llvm::Instruction::Add:  alive_op = BinOp::Add; break;
    case llvm::Instruction::Sub:  alive_op = BinOp::Sub; break;
    case llvm::Instruction::Mul:  alive_op = BinOp::Mul; break;
    case llvm::Instruction::SDiv: alive_op = BinOp::SDiv; break;
    case llvm::Instruction::UDiv: alive_op = BinOp::UDiv; break;
    case llvm::Instruction::SRem: alive_op = BinOp::SRem; break;
    case llvm::Instruction::URem: alive_op = BinOp::URem; break;
    case llvm::Instruction::Shl:  alive_op = BinOp::Shl; break;
    case llvm::Instruction::AShr: alive_op = BinOp::AShr; break;
    case llvm::Instruction::LShr: alive_op = BinOp::LShr; break;
    case llvm::Instruction::And:  alive_op = BinOp::And; break;
    case llvm::Instruction::Or:   alive_op = BinOp::Or; break;
    case llvm::Instruction::Xor:  alive_op = BinOp::Xor; break;
    case llvm::Instruction::FAdd: fp_op = FpBinOp::FAdd; is_fp = true; break;
    case llvm::Instruction::FSub: fp_op = FpBinOp::FSub; is_fp = true; break;
    case llvm::Instruction::FMul: fp_op = FpBinOp::FMul; is_fp = true; break;
    case llvm::Instruction::FDiv: fp_op = FpBinOp::FDiv; is_fp = true; break;
    case llvm::Instruction::FRem: fp_op = FpBinOp::FRem; is_fp = true; break;
    default:
      return error(i);
    }

    if (is_fp)
      RETURN_IDENTIFIER(make_unique<FpBinOp>(*ty, value_name(i), *a, *b, fp_op,
                                             parse_fmath(i)));

    unsigned flags = BinOp::None;
    if (isa<llvm::OverflowingBinaryOperator>(i) && i.hasNoSignedWrap())
      flags |= BinOp::NSW;
    if (isa<llvm::OverflowingBinaryOperator>(i) && i.hasNoUnsignedWrap())
      flags |= BinOp::NUW;
    if (isa<llvm::PossiblyExactOperator>(i) && i.isExact())
      flags = BinOp::Exact;
    RETURN_IDENTIFIER(make_unique<BinOp>(*ty, value_name(i), *a, *b, alive_op,
                                         flags));
  }

  RetTy visitCastInst(llvm::CastInst &i) {
    PARSE_UNOP();
    {
      ConversionOp::Op op;
      bool has_non_fp = true;
      switch (i.getOpcode()) {
      case llvm::Instruction::SExt:     op = ConversionOp::SExt; break;
      case llvm::Instruction::ZExt:     op = ConversionOp::ZExt; break;
      case llvm::Instruction::Trunc:    op = ConversionOp::Trunc; break;
      case llvm::Instruction::BitCast:  op = ConversionOp::BitCast; break;
      case llvm::Instruction::PtrToInt: op = ConversionOp::Ptr2Int; break;
      case llvm::Instruction::IntToPtr: op = ConversionOp::Int2Ptr; break;
      default: has_non_fp = false; break;
      }
      if (has_non_fp)
        RETURN_IDENTIFIER(
          make_unique<ConversionOp>(*ty, value_name(i), *val, op));
    }

    FpConversionOp::Op op;
    switch (i.getOpcode()) {
    case llvm::Instruction::SIToFP:  op = FpConversionOp::SIntToFP; break;
    case llvm::Instruction::UIToFP:  op = FpConversionOp::UIntToFP; break;
    case llvm::Instruction::FPToSI:  op = FpConversionOp::FPToSInt; break;
    case llvm::Instruction::FPToUI:  op = FpConversionOp::FPToUInt; break;
    case llvm::Instruction::FPExt:   op = FpConversionOp::FPExt; break;
    case llvm::Instruction::FPTrunc: op = FpConversionOp::FPTrunc; break;
    default:
      return error(i);
    }
    RETURN_IDENTIFIER(
      make_unique<FpConversionOp>(*ty, value_name(i), *val, op));
  }

  RetTy visitFreezeInst(llvm::FreezeInst &i) {
    PARSE_UNOP();
    RETURN_IDENTIFIER(make_unique<Freeze>(*ty, value_name(i), *val));
  }

  RetTy visitCallInst(llvm::CallInst &i, bool approx = false) {
    vector<Value *> args;
    for (auto &arg : i.args()) {
      auto a = get_operand(arg);
      if (!a)
        return error(i);
      args.emplace_back(a);
    }

    FnAttrs attrs;
    vector<ParamAttrs> param_attrs;
    if (!approx) {
      auto [known, attrs0, params0, approx0] = known_call(i, TLI, *BB, args);
      if (known)
        RETURN_IDENTIFIER(std::move(known));

      attrs       = std::move(attrs0);
      param_attrs = std::move(params0);
      approx      = approx0;
    }

    auto ty = llvm_type2alive(i.getType());
    if (!ty)
      return error(i);

    unique_ptr<FnCall> call;
    auto fn = i.getCalledFunction();

    if (auto *iasm = dyn_cast<llvm::InlineAsm>(i.getCalledOperand())) {
      assert(!approx);
      if (!iasm->canThrow())
        attrs.set(FnAttrs::NoThrow);
      call = make_unique<InlineAsm>(*ty, value_name(i), iasm->getAsmString(),
                                    iasm->getConstraintString(), std::move(attrs));
    } else {
      if (!fn) // TODO: support indirect calls
        return error(i);

      if (fn->getName().substr(0, 15) == "__llvm_profile_")
        return NOP(i);
    }

    llvm::AttributeList attrs_callsite = i.getAttributes();
    llvm::AttributeList attrs_fndef = fn ? fn->getAttributes()
                                         : llvm::AttributeList();

    parse_fn_attrs(i, attrs);

    if (auto op = dyn_cast<llvm::FPMathOperator>(&i)) {
      if (op->hasNoNaNs())
        attrs.set(FnAttrs::NNaN);
    }

    if (fn)
      call = make_unique<FnCall>(*ty, value_name(i),
                                 '@' + fn->getName().str(), std::move(attrs));
    unique_ptr<Instr> ret_val;

    for (uint64_t argidx = 0, nargs = i.arg_size(); argidx < nargs; ++argidx) {
      auto *arg = args[argidx];
      ParamAttrs pattr = argidx < param_attrs.size() ? param_attrs[argidx]
                                                     : ParamAttrs();

      unsigned attr_argidx = llvm::AttributeList::FirstArgIndex + argidx;
      approx |= !handleParamAttrs(attrs_callsite.getAttributes(attr_argidx),
                                  pattr, true);
      if (fn && argidx < fn->arg_size())
        approx |= !handleParamAttrs(attrs_fndef.getAttributes(attr_argidx),
                                    pattr, true);

      if (i.paramHasAttr(argidx, llvm::Attribute::NoUndef)) {
        if (i.getArgOperand(argidx)->getType()->isAggregateType())
          // TODO: noundef aggregate should be supported; it can have undef
          // padding
          return errorAttr(i.getAttributeAtIndex(argidx, llvm::Attribute::NoUndef));
      }

      if (i.paramHasAttr(argidx, llvm::Attribute::Returned)) {
        auto call2
          = make_unique<FnCall>(Type::voidTy, "", string(call->getFnName()),
                                FnAttrs(call->getAttributes()));
        for (auto &[arg, flags] : call->getArgs()) {
          call2->addArg(*arg, ParamAttrs(flags));
        }
        call = std::move(call2);

        // fn may have different type than argument. LLVM assumes there's
        // an implicit bitcast
        assert(!ret_val);
        if (i.getArgOperand(argidx)->getType() == i.getType())
          ret_val = make_unique<UnaryOp>(*ty, value_name(i), *arg,
                                          UnaryOp::Copy);
        else
          ret_val = make_unique<ConversionOp>(*ty, value_name(i), *arg,
                                              ConversionOp::BitCast);
      }

      call->addArg(*arg, std::move(pattr));
    }

    call->setApproximated(approx);

    if (ret_val) {
      BB->addInstr(std::move(call));
      RETURN_IDENTIFIER(std::move(ret_val));
    }
    RETURN_IDENTIFIER(std::move(call));
  }

  RetTy visitMemSetInst(llvm::MemSetInst &i) {
    auto ptr = get_operand(i.getRawDest());
    auto val = get_operand(i.getValue());
    auto bytes = get_operand(i.getLength());
    // TODO: add isvolatile
    if (!ptr || !val || !bytes)
      return error(i);

    RETURN_IDENTIFIER(make_unique<Memset>(*ptr, *val, *bytes,
                                          max(1u, i.getDestAlignment())));
  }

  RetTy visitMemTransferInst(llvm::MemTransferInst &i) {
    auto dst = get_operand(i.getRawDest());
    auto src = get_operand(i.getRawSource());
    auto bytes = get_operand(i.getLength());
    // TODO: add isvolatile
    if (!dst || !src || !bytes)
      return error(i);

    RETURN_IDENTIFIER(make_unique<Memcpy>(*dst, *src, *bytes,
                                          max(1u, i.getDestAlignment()),
                                          max(1u, i.getSourceAlignment()),
                                          isa<llvm::MemMoveInst>(&i)));
  }

  RetTy visitICmpInst(llvm::ICmpInst &i) {
    PARSE_BINOP();
    ICmp::Cond cond;
    switch (i.getPredicate()) {
    case llvm::CmpInst::ICMP_EQ:  cond = ICmp::EQ; break;
    case llvm::CmpInst::ICMP_NE:  cond = ICmp::NE; break;
    case llvm::CmpInst::ICMP_UGT: cond = ICmp::UGT; break;
    case llvm::CmpInst::ICMP_UGE: cond = ICmp::UGE; break;
    case llvm::CmpInst::ICMP_ULT: cond = ICmp::ULT; break;
    case llvm::CmpInst::ICMP_ULE: cond = ICmp::ULE; break;
    case llvm::CmpInst::ICMP_SGT: cond = ICmp::SGT; break;
    case llvm::CmpInst::ICMP_SGE: cond = ICmp::SGE; break;
    case llvm::CmpInst::ICMP_SLT: cond = ICmp::SLT; break;
    case llvm::CmpInst::ICMP_SLE: cond = ICmp::SLE; break;
    default:
      UNREACHABLE();
    }
    RETURN_IDENTIFIER(make_unique<ICmp>(*ty, value_name(i), cond, *a, *b));
  }

  RetTy visitFCmpInst(llvm::FCmpInst &i) {
    PARSE_BINOP();
    FCmp::Cond cond;
    switch (i.getPredicate()) {
    case llvm::CmpInst::FCMP_OEQ:   cond = FCmp::OEQ; break;
    case llvm::CmpInst::FCMP_OGT:   cond = FCmp::OGT; break;
    case llvm::CmpInst::FCMP_OGE:   cond = FCmp::OGE; break;
    case llvm::CmpInst::FCMP_OLT:   cond = FCmp::OLT; break;
    case llvm::CmpInst::FCMP_OLE:   cond = FCmp::OLE; break;
    case llvm::CmpInst::FCMP_ONE:   cond = FCmp::ONE; break;
    case llvm::CmpInst::FCMP_ORD:   cond = FCmp::ORD; break;
    case llvm::CmpInst::FCMP_UEQ:   cond = FCmp::UEQ; break;
    case llvm::CmpInst::FCMP_UGT:   cond = FCmp::UGT; break;
    case llvm::CmpInst::FCMP_UGE:   cond = FCmp::UGE; break;
    case llvm::CmpInst::FCMP_ULT:   cond = FCmp::ULT; break;
    case llvm::CmpInst::FCMP_ULE:   cond = FCmp::ULE; break;
    case llvm::CmpInst::FCMP_UNE:   cond = FCmp::UNE; break;
    case llvm::CmpInst::FCMP_UNO:   cond = FCmp::UNO; break;
    case llvm::CmpInst::FCMP_TRUE:  cond = FCmp::TRUE; break;
    case llvm::CmpInst::FCMP_FALSE: cond = FCmp::FALSE; break;
    default:
      UNREACHABLE();
    }
    RETURN_IDENTIFIER(make_unique<FCmp>(*ty, value_name(i), cond, *a, *b,
                                        parse_fmath(i)));
  }

  RetTy visitSelectInst(llvm::SelectInst &i) {
    PARSE_TRIOP();
    RETURN_IDENTIFIER(make_unique<Select>(*ty, value_name(i), *a, *b, *c,
                                          parse_fmath(i)));
  }

  RetTy visitExtractValueInst(llvm::ExtractValueInst &i) {
    auto ty = llvm_type2alive(i.getType());
    auto val = get_operand(i.getAggregateOperand());
    if (!ty || !val)
      return error(i);

    auto inst = make_unique<ExtractValue>(*ty, value_name(i), *val);

    auto *valty = &val->getType();
    for (auto idx : i.indices()) {
      auto *aty = valty->getAsAggregateType();
      unsigned idx_with_paddings = aty->countPaddings(idx) + idx;
      inst->addIdx(idx_with_paddings);
      valty = &aty->getChild(idx_with_paddings);
    }

    RETURN_IDENTIFIER(std::move(inst));
  }

  RetTy visitInsertValueInst(llvm::InsertValueInst &i) {
    auto ty = llvm_type2alive(i.getType());
    auto val = get_operand(i.getAggregateOperand());
    auto elt = get_operand(i.getInsertedValueOperand());
    if (!ty || !val || !elt)
      return error(i);

    auto inst = make_unique<InsertValue>(*ty, value_name(i), *val, *elt);

    for (auto idx : i.indices()) {
      auto *aty = ty->getAsAggregateType();
      unsigned idx_with_paddings = aty->countPaddings(idx) + idx;
      inst->addIdx(idx_with_paddings);
      ty = &aty->getChild(idx_with_paddings);
    }

    RETURN_IDENTIFIER(std::move(inst));
  }

  RetTy visitAllocaInst(llvm::AllocaInst &i) {
    auto ty = llvm_type2alive(i.getType());
    if (!ty)
      return error(i);

    Value *mul = nullptr;
    if (i.isArrayAllocation()) {
      if (!(mul = get_operand(i.getArraySize())))
        return error(i);
    }

    auto typesz = DL().getTypeAllocSize(i.getAllocatedType());
    if (typesz.isScalable()) // TODO: scalable vectors not supported
      return error(i);

    auto size = make_intconst(typesz, 64);
    auto alloc = make_unique<Alloc>(*ty, value_name(i), *size, mul,
                      pref_alignment(i, i.getAllocatedType()));
    allocs.emplace(&i, make_pair(alloc.get(), /*has lifetime.start?*/ false));
    RETURN_IDENTIFIER(std::move(alloc));
  }

  RetTy visitGetElementPtrInst(llvm::GetElementPtrInst &i) {
    auto ty = llvm_type2alive(i.getType());
    auto ptr = get_operand(i.getPointerOperand());
    if (!ty || !ptr)
      return error(i);

    auto gep = make_unique<GEP>(*ty, value_name(i), *ptr, i.isInBounds());
    auto gep_struct_ofs = [&i, this](llvm::StructType *sty, llvm::Value *ofs) {
      llvm::Value *vals[] = { llvm::ConstantInt::getFalse(i.getContext()), ofs };
      return this->DL().getIndexedOffsetInType(sty,
          llvm::makeArrayRef(vals, 2));
    };

    // reference: GEPOperator::accumulateConstantOffset()
    for (auto I = llvm::gep_type_begin(i), E = llvm::gep_type_end(i);
         I != E; ++I) {
      auto op = get_operand(I.getOperand());
      if (!op)
        return error(i);

      if (auto structTy = I.getStructTypeOrNull()) {
        auto opty = I.getOperand()->getType();
        auto ofs_ty = llvm::IntegerType::get(i.getContext(), 64);

        if (auto opvty = dyn_cast<llvm::VectorType>(opty)) {
          assert(!isa<llvm::ScalableVectorType>(opvty));
          vector<llvm::Constant *> offsets;

          for (unsigned i = 0; i < opvty->getElementCount().getKnownMinValue();
               ++i) {
            llvm::Constant *constofs = nullptr;
            if (auto cdv = dyn_cast<llvm::ConstantDataVector>(I.getOperand())) {
              constofs = cdv->getElementAsConstant(i);
            } else if (auto cv = dyn_cast<llvm::ConstantAggregateZero>(
                I.getOperand())) {
              constofs = cv->getSequentialElement();
            } else {
              assert(false);
            }
            offsets.push_back(llvm::ConstantInt::get(ofs_ty,
                (uint64_t)gep_struct_ofs(structTy, constofs)));
          }

          auto ofs_vector = llvm::ConstantVector::get(
              llvm::makeArrayRef(offsets.data(), offsets.size()));
          gep->addIdx(1, *get_operand(ofs_vector));
        } else {
          gep->addIdx(1, *make_intconst(
              gep_struct_ofs(structTy, I.getOperand()), 64));
        }
        continue;
      }

      gep->addIdx(DL().getTypeAllocSize(I.getIndexedType()).getKnownMinValue(),
                  *op);
    }
    RETURN_IDENTIFIER(std::move(gep));
  }

  RetTy visitLoadInst(llvm::LoadInst &i) {
    // TODO: Add support for isVolatile(), getOrdering()
    if (!i.isSimple())
      return error(i);
    PARSE_UNOP();
    RETURN_IDENTIFIER(make_unique<Load>(*ty, value_name(i), *val,
                                        alignment(i, i.getType())));
  }

  RetTy visitStoreInst(llvm::StoreInst &i) {
    // TODO: Add support for isVolatile(), getOrdering()
    if (!i.isSimple())
      return error(i);
    auto val = get_operand(i.getValueOperand());
    auto ptr = get_operand(i.getPointerOperand());
    if (!val || !ptr)
      return error(i);
    RETURN_IDENTIFIER(make_unique<Store>(*ptr, *val,
                        alignment(i, i.getValueOperand()->getType())));
  }

  RetTy visitPHINode(llvm::PHINode &i) {
    auto ty = llvm_type2alive(i.getType());
    if (!ty)
      return error(i);

    auto phi = make_unique<Phi>(*ty, value_name(i), parse_fmath(i));
    todo_phis.emplace_back(phi.get(), &i);
    RETURN_IDENTIFIER(std::move(phi));
  }

  RetTy visitBranchInst(llvm::BranchInst &i) {
    auto &dst_true = getBB(i.getSuccessor(0));
    if (i.isUnconditional())
      return make_unique<Branch>(dst_true);

    auto &dst_false = getBB(i.getSuccessor(1));
    auto cond = get_operand(i.getCondition());
    if (!cond)
      return error(i);
    return make_unique<Branch>(*cond, dst_true, dst_false);
  }

  RetTy visitSwitchInst(llvm::SwitchInst &i) {
    auto cond = get_operand(i.getCondition());
    if (!cond)
      return error(i);
    auto op = make_unique<Switch>(*cond, getBB(i.getDefaultDest()));

    for (auto &Case : i.cases()) {
      op->addTarget(*get_operand(Case.getCaseValue()),
                    getBB(Case.getCaseSuccessor()));
    }
    return op;
  }

  RetTy visitReturnInst(llvm::ReturnInst &i) {
    if (i.getNumOperands() == 0) {
      assert(i.getType()->isVoidTy());
      return make_unique<Return>(Type::voidTy, Value::voidVal);
    }

    auto ty = llvm_type2alive(i.getOperand(0)->getType());
    auto val = get_operand(i.getOperand(0));
    if (!ty || !val)
      return error(i);

    return make_unique<Return>(*ty, *val);
  }

  RetTy visitUnreachableInst(llvm::UnreachableInst &i) {
    auto fals = get_operand(llvm::ConstantInt::getFalse(i.getContext()));
    return make_unique<Assume>(*fals, Assume::AndNonPoison);
  }

  enum LifetimeKind {
    LIFETIME_START,
    LIFETIME_START_FILLPOISON,
    LIFETIME_FILLPOISON,
    LIFETIME_FREE,
    LIFETIME_NOP,
    LIFETIME_UNKNOWN
  };
  LifetimeKind getLifetimeKind(llvm::IntrinsicInst &i) {
    llvm::Value *Ptr = i.getOperand(1);

    if (isa<llvm::UndefValue>(Ptr))
      // lifetime with undef ptr is no-op
      return LIFETIME_NOP;

    llvm::SmallVector<const llvm::Value *> Objs;
    llvm::getUnderlyingObjects(Ptr, Objs);

    if (llvm::all_of(Objs, [this](const llvm::Value *V) {
        // Stack coloring algorithm doesn't assign slots for global variables
        // or objects passed as pointer arguments
        return llvm::isa<llvm::Argument>(V) ||
               llvm::isa<llvm::GlobalVariable>(V) ||
               llvm::isAllocLikeFn(V, &TLI); }))
      return LIFETIME_FILLPOISON;

    Objs.clear();
    // Restricted to ops without ptr offset changes only
    Ptr = Ptr->stripPointerCasts();
    if (auto *Sel = dyn_cast<llvm::SelectInst>(Ptr)) {
      Objs.emplace_back(Sel->getTrueValue());
      Objs.emplace_back(Sel->getFalseValue());
    } else if (auto *Phi = dyn_cast<llvm::PHINode>(Ptr)) {
      for (auto &U : Phi->incoming_values())
        Objs.emplace_back(U.get());
    } else
      Objs.emplace_back(Ptr);

    if (!llvm::all_of(Objs, [](const llvm::Value *V) {
         return llvm::isa<llvm::AllocaInst>(V->stripPointerCasts()); })) {
      // If it is gep(alloca, const) where const != 0, it is fillpoison.
      // Since such pattern is rare, check the simplest case and just return
      // unknown otherwise
      if (Objs.size() == 1) {
        const auto *GEPI = llvm::dyn_cast<llvm::GetElementPtrInst>(Objs[0]);
        if (GEPI && llvm::isa<llvm::AllocaInst>(
                            GEPI->getPointerOperand()->stripPointerCasts())) {
          unsigned BitWidth = DL().getIndexTypeSizeInBits(GEPI->getType());
          llvm::APInt Offset(BitWidth, 0);
          if (GEPI->accumulateConstantOffset(DL(), Offset) &&
              !Offset.isNullValue())
            return LIFETIME_FILLPOISON;
        }
      }
      return LIFETIME_UNKNOWN;
    }

    // Now, it is guaranteed that Ptr points to alloca with zero offset.

    if (i.getIntrinsicID() == llvm::Intrinsic::lifetime_end)
      // double free is okay
      return LIFETIME_FREE;

    // lifetime.start
    bool needs_fillpoison = false;

    for (const auto *V : Objs) {
      auto *ai = cast<llvm::AllocaInst>(V);
      auto itr = allocs.find(ai);
      if (itr == allocs.end())
        // AllocaInst isn't visited; it is in a loop.
        return LIFETIME_UNKNOWN;

      itr->second.first->markAsInitiallyDead();
      if (itr->second.second) {
        // it isn't the first lifetime.start; conservatively add fillpoison
        // to correctly encode the precise semantics
        needs_fillpoison = true;
      }
      itr->second.second = true;
    }
    return needs_fillpoison ? LIFETIME_START_FILLPOISON : LIFETIME_START;
  }

  RetTy visitIntrinsicInst(llvm::IntrinsicInst &i) {
    switch (i.getIntrinsicID()) {
    case llvm::Intrinsic::assume:
    {
      unsigned n = i.getNumOperandBundles();
      for (unsigned j = 0; j < n; ++j) {
        auto bundle = i.getOperandBundleAt(j);
        llvm::StringRef name = bundle.getTagName();
        if (name == "noundef") {
          // Currently it is unclear whether noundef bundle takes single op.
          // Be conservative & simply assume it can take arbitrary num of ops.
          for (unsigned j = 0; j < bundle.Inputs.size(); ++j) {
            llvm::Value *v = bundle.Inputs[j].get();
            auto *av = get_operand(v);
            if (!av)
              return error(i);

            BB->addInstr(
              make_unique<Assume>(*av, Assume::WellDefined));
          }
        } else if (name == "align") {
          llvm::Value *ptr = bundle.Inputs[0].get();
          llvm::Value *align = bundle.Inputs[1].get();
          auto *aptr = get_operand(ptr), *aalign = get_operand(align);
          if (!aptr || !aalign || align->getType()->getIntegerBitWidth() > 64)
            return error(i);

          if (bundle.Inputs.size() >= 3) {
            assert(bundle.Inputs.size() == 3);
            auto gep = make_unique<GEP>(
                aptr->getType(),
                "#align_adjustedptr" + to_string(alignopbundle_idx++),
                *aptr, false);
            gep->addIdx(-1ull, *get_operand(bundle.Inputs[2].get()));

            aptr = gep.get();
            BB->addInstr(std::move(gep));
          }

          vector<Value *> args = {aptr, aalign};
          BB->addInstr(make_unique<Assume>(std::move(args), Assume::Align));
        } else if (name == "nonnull") {
          llvm::Value *ptr = bundle.Inputs[0].get();
          auto *aptr = get_operand(ptr);
          if (!aptr)
            return error(i);

          BB->addInstr(
            make_unique<Assume>(*aptr, Assume::NonNull));
        } else {
          return error(i);
        }
      }
      PARSE_UNOP();
      return make_unique<Assume>(*val, Assume::AndNonPoison);
    }
    case llvm::Intrinsic::sadd_with_overflow:
    case llvm::Intrinsic::uadd_with_overflow:
    case llvm::Intrinsic::ssub_with_overflow:
    case llvm::Intrinsic::usub_with_overflow:
    case llvm::Intrinsic::smul_with_overflow:
    case llvm::Intrinsic::umul_with_overflow:
    case llvm::Intrinsic::sadd_sat:
    case llvm::Intrinsic::uadd_sat:
    case llvm::Intrinsic::ssub_sat:
    case llvm::Intrinsic::usub_sat:
    case llvm::Intrinsic::sshl_sat:
    case llvm::Intrinsic::ushl_sat:
    case llvm::Intrinsic::cttz:
    case llvm::Intrinsic::ctlz:
    case llvm::Intrinsic::umin:
    case llvm::Intrinsic::umax:
    case llvm::Intrinsic::smin:
    case llvm::Intrinsic::smax:
    case llvm::Intrinsic::abs:
    {
      PARSE_BINOP();
      BinOp::Op op;
      switch (i.getIntrinsicID()) {
      case llvm::Intrinsic::sadd_with_overflow: op=BinOp::SAdd_Overflow; break;
      case llvm::Intrinsic::uadd_with_overflow: op=BinOp::UAdd_Overflow; break;
      case llvm::Intrinsic::ssub_with_overflow: op=BinOp::SSub_Overflow; break;
      case llvm::Intrinsic::usub_with_overflow: op=BinOp::USub_Overflow; break;
      case llvm::Intrinsic::smul_with_overflow: op=BinOp::SMul_Overflow; break;
      case llvm::Intrinsic::umul_with_overflow: op=BinOp::UMul_Overflow; break;
      case llvm::Intrinsic::sadd_sat: op = BinOp::SAdd_Sat; break;
      case llvm::Intrinsic::uadd_sat: op = BinOp::UAdd_Sat; break;
      case llvm::Intrinsic::ssub_sat: op = BinOp::SSub_Sat; break;
      case llvm::Intrinsic::usub_sat: op = BinOp::USub_Sat; break;
      case llvm::Intrinsic::sshl_sat: op = BinOp::SShl_Sat; break;
      case llvm::Intrinsic::ushl_sat: op = BinOp::UShl_Sat; break;
      case llvm::Intrinsic::cttz:     op = BinOp::Cttz; break;
      case llvm::Intrinsic::ctlz:     op = BinOp::Ctlz; break;
      case llvm::Intrinsic::umin:     op = BinOp::UMin; break;
      case llvm::Intrinsic::umax:     op = BinOp::UMax; break;
      case llvm::Intrinsic::smin:     op = BinOp::SMin; break;
      case llvm::Intrinsic::smax:     op = BinOp::SMax; break;
      case llvm::Intrinsic::abs:      op = BinOp::Abs; break;
      default: UNREACHABLE();
      }
      RETURN_IDENTIFIER(make_unique<BinOp>(*ty, value_name(i), *a, *b, op));
    }
    case llvm::Intrinsic::bitreverse:
    case llvm::Intrinsic::bswap:
    case llvm::Intrinsic::ctpop:
    case llvm::Intrinsic::expect:
    case llvm::Intrinsic::expect_with_probability:
    case llvm::Intrinsic::is_constant: {
      PARSE_UNOP();
      UnaryOp::Op op;
      switch (i.getIntrinsicID()) {
      case llvm::Intrinsic::bitreverse:  op = UnaryOp::BitReverse; break;
      case llvm::Intrinsic::bswap:       op = UnaryOp::BSwap; break;
      case llvm::Intrinsic::ctpop:       op = UnaryOp::Ctpop; break;
      case llvm::Intrinsic::expect:
      case llvm::Intrinsic::expect_with_probability:
        op = UnaryOp::Copy; break;
      case llvm::Intrinsic::is_constant: op = UnaryOp::IsConstant; break;
      default: UNREACHABLE();
      }
      RETURN_IDENTIFIER(make_unique<UnaryOp>(*ty, value_name(i), *val, op));
    }
    case llvm::Intrinsic::vector_reduce_add:
    case llvm::Intrinsic::vector_reduce_mul:
    case llvm::Intrinsic::vector_reduce_and:
    case llvm::Intrinsic::vector_reduce_or:
    case llvm::Intrinsic::vector_reduce_xor:
    case llvm::Intrinsic::vector_reduce_smax:
    case llvm::Intrinsic::vector_reduce_smin:
    case llvm::Intrinsic::vector_reduce_umax:
    case llvm::Intrinsic::vector_reduce_umin: {
      PARSE_UNOP();
      UnaryReductionOp::Op op;
      switch (i.getIntrinsicID()) {
      case llvm::Intrinsic::vector_reduce_add: op = UnaryReductionOp::Add; break;
      case llvm::Intrinsic::vector_reduce_mul: op = UnaryReductionOp::Mul; break;
      case llvm::Intrinsic::vector_reduce_and: op = UnaryReductionOp::And; break;
      case llvm::Intrinsic::vector_reduce_or:  op = UnaryReductionOp::Or;  break;
      case llvm::Intrinsic::vector_reduce_xor: op = UnaryReductionOp::Xor; break;
      case llvm::Intrinsic::vector_reduce_smax: op = UnaryReductionOp::SMax; break;
      case llvm::Intrinsic::vector_reduce_smin: op = UnaryReductionOp::SMin; break;
      case llvm::Intrinsic::vector_reduce_umax: op = UnaryReductionOp::UMax; break;
      case llvm::Intrinsic::vector_reduce_umin: op = UnaryReductionOp::UMin; break;
      default: UNREACHABLE();
      }
      RETURN_IDENTIFIER(
        make_unique<UnaryReductionOp>(*ty, value_name(i), *val, op));
    }
    case llvm::Intrinsic::fshl:
    case llvm::Intrinsic::fshr:
    case llvm::Intrinsic::smul_fix:
    case llvm::Intrinsic::umul_fix:
    case llvm::Intrinsic::smul_fix_sat:
    case llvm::Intrinsic::umul_fix_sat:
    {
      PARSE_TRIOP();
      TernaryOp::Op op;
      switch (i.getIntrinsicID()) {
      case llvm::Intrinsic::fshl: op = TernaryOp::FShl; break;
      case llvm::Intrinsic::fshr: op = TernaryOp::FShr; break;
      case llvm::Intrinsic::smul_fix: op = TernaryOp::SMulFix; break;
      case llvm::Intrinsic::umul_fix: op = TernaryOp::UMulFix; break;
      case llvm::Intrinsic::smul_fix_sat: op = TernaryOp::SMulFixSat; break;
      case llvm::Intrinsic::umul_fix_sat: op = TernaryOp::UMulFixSat; break;
      default: UNREACHABLE();
      }
      RETURN_IDENTIFIER(
        make_unique<TernaryOp>(*ty, value_name(i), *a, *b, *c, op));
    }
    case llvm::Intrinsic::fma:
    case llvm::Intrinsic::fmuladd:
    case llvm::Intrinsic::experimental_constrained_fma:
    case llvm::Intrinsic::experimental_constrained_fmuladd:
    {
      PARSE_TRIOP();
      FpTernaryOp::Op op;
      switch (i.getIntrinsicID()) {
      case llvm::Intrinsic::fma:
      case llvm::Intrinsic::experimental_constrained_fma:     op = FpTernaryOp::FMA; break;
      case llvm::Intrinsic::fmuladd:
      case llvm::Intrinsic::experimental_constrained_fmuladd: op = FpTernaryOp::MulAdd; break;
      default: UNREACHABLE();
      }
      RETURN_IDENTIFIER(
        make_unique<FpTernaryOp>(*ty, value_name(i), *a, *b, *c, op,
                                 parse_fmath(i), parse_rounding(i),
                                 parse_exceptions(i)));
    }
    case llvm::Intrinsic::copysign:
    case llvm::Intrinsic::minnum:
    case llvm::Intrinsic::maxnum:
    case llvm::Intrinsic::minimum:
    case llvm::Intrinsic::maximum:
    case llvm::Intrinsic::experimental_constrained_fadd:
    case llvm::Intrinsic::experimental_constrained_fsub:
    case llvm::Intrinsic::experimental_constrained_fmul:
    case llvm::Intrinsic::experimental_constrained_fdiv:
    case llvm::Intrinsic::experimental_constrained_minnum:
    case llvm::Intrinsic::experimental_constrained_maxnum:
    case llvm::Intrinsic::experimental_constrained_minimum:
    case llvm::Intrinsic::experimental_constrained_maximum:
    {
      PARSE_BINOP();
      FpBinOp::Op op;
      switch (i.getIntrinsicID()) {
      case llvm::Intrinsic::copysign:                         op = FpBinOp::CopySign; break;
      case llvm::Intrinsic::minnum:
      case llvm::Intrinsic::experimental_constrained_minnum:  op = FpBinOp::FMin; break;
      case llvm::Intrinsic::maxnum:
      case llvm::Intrinsic::experimental_constrained_maxnum:  op = FpBinOp::FMax; break;
      case llvm::Intrinsic::minimum:
      case llvm::Intrinsic::experimental_constrained_minimum: op = FpBinOp::FMinimum; break;
      case llvm::Intrinsic::maximum:
      case llvm::Intrinsic::experimental_constrained_maximum: op = FpBinOp::FMaximum; break;
      case llvm::Intrinsic::experimental_constrained_fadd:    op = FpBinOp::FAdd; break;
      case llvm::Intrinsic::experimental_constrained_fsub:    op = FpBinOp::FSub; break;
      case llvm::Intrinsic::experimental_constrained_fmul:    op = FpBinOp::FMul; break;
      case llvm::Intrinsic::experimental_constrained_fdiv:    op = FpBinOp::FDiv; break;
      default: UNREACHABLE();
      }
      RETURN_IDENTIFIER(
        make_unique<FpBinOp>(*ty, value_name(i), *a, *b, op, parse_fmath(i),
                             parse_rounding(i), parse_exceptions(i)));
    }
    case llvm::Intrinsic::fabs:
    case llvm::Intrinsic::ceil:
    case llvm::Intrinsic::experimental_constrained_ceil:
    case llvm::Intrinsic::floor:
    case llvm::Intrinsic::experimental_constrained_floor:
    case llvm::Intrinsic::rint:
    case llvm::Intrinsic::experimental_constrained_rint:
    case llvm::Intrinsic::nearbyint:
    case llvm::Intrinsic::experimental_constrained_nearbyint:
    case llvm::Intrinsic::round:
    case llvm::Intrinsic::experimental_constrained_round:
    case llvm::Intrinsic::roundeven:
    case llvm::Intrinsic::experimental_constrained_roundeven:
    case llvm::Intrinsic::sqrt:
    case llvm::Intrinsic::experimental_constrained_sqrt:
    case llvm::Intrinsic::trunc:
    case llvm::Intrinsic::experimental_constrained_trunc:
    {
      PARSE_UNOP();
      FpUnaryOp::Op op;
      switch (i.getIntrinsicID()) {
      case llvm::Intrinsic::fabs:                               op = FpUnaryOp::FAbs; break;
      case llvm::Intrinsic::ceil:
      case llvm::Intrinsic::experimental_constrained_ceil:      op = FpUnaryOp::Ceil; break;
      case llvm::Intrinsic::floor:
      case llvm::Intrinsic::experimental_constrained_floor:     op = FpUnaryOp::Floor; break;
      case llvm::Intrinsic::rint:
      case llvm::Intrinsic::experimental_constrained_rint:      op = FpUnaryOp::RInt; break;
      case llvm::Intrinsic::nearbyint:
      case llvm::Intrinsic::experimental_constrained_nearbyint: op = FpUnaryOp::NearbyInt; break;
      case llvm::Intrinsic::round:
      case llvm::Intrinsic::experimental_constrained_round:     op = FpUnaryOp::Round; break;
      case llvm::Intrinsic::roundeven:
      case llvm::Intrinsic::experimental_constrained_roundeven: op = FpUnaryOp::RoundEven; break;
      case llvm::Intrinsic::sqrt:
      case llvm::Intrinsic::experimental_constrained_sqrt:      op = FpUnaryOp::Sqrt; break;
      case llvm::Intrinsic::trunc:
      case llvm::Intrinsic::experimental_constrained_trunc:     op = FpUnaryOp::Trunc; break;
      default: UNREACHABLE();
      }
      RETURN_IDENTIFIER(
        make_unique<FpUnaryOp>(*ty, value_name(i), *val, op, parse_fmath(i),
                               parse_rounding(i), parse_exceptions(i)));
    }
    case llvm::Intrinsic::experimental_constrained_sitofp:
    case llvm::Intrinsic::experimental_constrained_uitofp:
    case llvm::Intrinsic::experimental_constrained_fptosi:
    case llvm::Intrinsic::experimental_constrained_fptoui:
    case llvm::Intrinsic::experimental_constrained_fpext:
    case llvm::Intrinsic::experimental_constrained_fptrunc:
    case llvm::Intrinsic::lrint:
    case llvm::Intrinsic::experimental_constrained_lrint:
    case llvm::Intrinsic::llrint:
    case llvm::Intrinsic::experimental_constrained_llrint:
    case llvm::Intrinsic::lround:
    case llvm::Intrinsic::experimental_constrained_lround:
    case llvm::Intrinsic::llround:
    case llvm::Intrinsic::experimental_constrained_llround:
    {
      PARSE_UNOP();
      FpConversionOp::Op op;
      switch (i.getIntrinsicID()) {
      case llvm::Intrinsic::experimental_constrained_sitofp:  op = FpConversionOp::SIntToFP; break;
      case llvm::Intrinsic::experimental_constrained_uitofp:  op = FpConversionOp::UIntToFP; break;
      case llvm::Intrinsic::experimental_constrained_fptosi:  op = FpConversionOp::FPToSInt; break;
      case llvm::Intrinsic::experimental_constrained_fptoui:  op = FpConversionOp::FPToUInt; break;
      case llvm::Intrinsic::experimental_constrained_fpext:   op = FpConversionOp::FPExt; break;
      case llvm::Intrinsic::experimental_constrained_fptrunc: op = FpConversionOp::FPTrunc; break;
      case llvm::Intrinsic::lrint:
      case llvm::Intrinsic::experimental_constrained_lrint:
      case llvm::Intrinsic::llrint:
      case llvm::Intrinsic::experimental_constrained_llrint:  op = FpConversionOp::LRInt; break;
      case llvm::Intrinsic::lround:
      case llvm::Intrinsic::experimental_constrained_lround:
      case llvm::Intrinsic::llround:
      case llvm::Intrinsic::experimental_constrained_llround: op = FpConversionOp::LRound; break;
      default: UNREACHABLE();
      }
      RETURN_IDENTIFIER(make_unique<FpConversionOp>(*ty, value_name(i), *val,
                                                    op, parse_rounding(i),
                                                    parse_exceptions(i)));
    }
    case llvm::Intrinsic::is_fpclass:
    {
      PARSE_BINOP();
      TestOp::Op op;
      switch (i.getIntrinsicID()) {
      case llvm::Intrinsic::is_fpclass: op = TestOp::Is_FPClass; break;
      default: UNREACHABLE();
      }
      RETURN_IDENTIFIER(make_unique<TestOp>(*ty, value_name(i), *a, *b, op));
    }
    case llvm::Intrinsic::lifetime_start:
    case llvm::Intrinsic::lifetime_end:
    {
      PARSE_BINOP();
      switch (getLifetimeKind(i)) {
      case LIFETIME_START:
        RETURN_IDENTIFIER(make_unique<StartLifetime>(*b));
      case LIFETIME_START_FILLPOISON:
        BB->addInstr(make_unique<StartLifetime>(*b));
        RETURN_IDENTIFIER(make_unique<FillPoison>(*b));
      case LIFETIME_FREE:
        RETURN_IDENTIFIER(make_unique<Free>(*b, false));
      case LIFETIME_FILLPOISON:
        RETURN_IDENTIFIER(make_unique<FillPoison>(*b));
      case LIFETIME_NOP:
        return NOP(i);
      case LIFETIME_UNKNOWN:
        return error(i);
      }
    }
    case llvm::Intrinsic::sideeffect: {
      FnAttrs attrs;
      parse_fn_attrs(i, attrs);
      attrs.set(FnAttrs::InaccessibleMemOnly);
      attrs.set(FnAttrs::WillReturn);
      attrs.set(FnAttrs::NoThrow);
      return make_unique<FnCall>(Type::voidTy, "", "#sideeffect", std::move(attrs));
    }
    case llvm::Intrinsic::trap: {
      FnAttrs attrs;
      parse_fn_attrs(i, attrs);
      attrs.set(FnAttrs::NoReturn);
      attrs.set(FnAttrs::NoThrow);
      return make_unique<FnCall>(Type::voidTy, "", "#trap", std::move(attrs));
    }
    case llvm::Intrinsic::vastart: {
      PARSE_UNOP();
      return make_unique<VaStart>(*val);
    }
    case llvm::Intrinsic::vaend: {
      PARSE_UNOP();
      return make_unique<VaEnd>(*val);
    }
    case llvm::Intrinsic::vacopy: {
      PARSE_BINOP();
      return make_unique<VaCopy>(*a, *b);
    }

    // do nothing intrinsics
    case llvm::Intrinsic::dbg_addr:
    case llvm::Intrinsic::dbg_declare:
    case llvm::Intrinsic::dbg_label:
    case llvm::Intrinsic::dbg_value:
    case llvm::Intrinsic::donothing:
    case llvm::Intrinsic::instrprof_increment:
    case llvm::Intrinsic::instrprof_increment_step:
    case llvm::Intrinsic::instrprof_value_profile:
    case llvm::Intrinsic::prefetch:
      return NOP(i);

    default:
      break;
    }
    return visitCallInst(i, true);
  }

  RetTy visitExtractElementInst(llvm::ExtractElementInst &i) {
    PARSE_BINOP();
    RETURN_IDENTIFIER(make_unique<ExtractElement>(*ty, value_name(i), *a, *b));
  }

  RetTy visitInsertElementInst(llvm::InsertElementInst &i) {
    PARSE_TRIOP();
    RETURN_IDENTIFIER(make_unique<InsertElement>(*ty, value_name(i), *a, *b,
                                                 *c));
  }

  RetTy visitShuffleVectorInst(llvm::ShuffleVectorInst &i) {
    PARSE_BINOP();
    vector<unsigned> mask;
    for (auto m : i.getShuffleMask())
      mask.push_back(m);
    RETURN_IDENTIFIER(make_unique<ShuffleVector>(*ty, value_name(i), *a, *b,
                                                 std::move(mask)));
  }

  RetTy visitVAArg(llvm::VAArgInst &i) {
    PARSE_UNOP();
    RETURN_IDENTIFIER(make_unique<VaArg>(*ty, value_name(i), *val));
  }

  RetTy visitInstruction(llvm::Instruction &i) { return error(i); }

  RetTy error(llvm::Instruction &i) {
    *out << "ERROR: Unsupported instruction: " << i << '\n';
    return {};
  }
  RetTy errorAttr(const llvm::Attribute &attr) {
    *out << "ERROR: Unsupported attribute: " << attr.getAsString() << '\n';
    return {};
  }

  bool handleMetadata(llvm::Instruction &llvm_i, Instr &i) {
    llvm::SmallVector<pair<unsigned, llvm::MDNode*>, 8> MDs;
    llvm_i.getAllMetadataOtherThanDebugLoc(MDs);

    for (auto &[ID, Node] : MDs) {
      switch (ID) {
      case LLVMContext::MD_range:
      {
        Value *range = nullptr;
        auto &boolTy = get_int_type(1);
        for (unsigned op = 0, e = Node->getNumOperands(); op < e; ++op) {
          auto *low =
            llvm::mdconst::extract<llvm::ConstantInt>(Node->getOperand(op));
          auto *high =
            llvm::mdconst::extract<llvm::ConstantInt>(Node->getOperand(++op));

          auto op_name = to_string(op / 2);
          auto l = make_unique<ICmp>(boolTy,
                                     "%range_l#" + op_name + value_name(llvm_i),
                                     ICmp::SGE, i, *get_operand(low));

          auto h = make_unique<ICmp>(boolTy,
                                     "%range_h#" + op_name + value_name(llvm_i),
                                     ICmp::SLT, i, *get_operand(high));

          bool wrap = low->getValue().sgt(high->getValue());
          auto r = make_unique<BinOp>(boolTy,
                                      "%range#" + op_name + value_name(llvm_i),
                                      *l.get(), *h.get(),
                                      wrap ? BinOp::Or : BinOp::And);

          auto r_ptr = r.get();
          BB->addInstr(std::move(l));
          BB->addInstr(std::move(h));
          BB->addInstr(std::move(r));

          if (range) {
            auto range_or = make_unique<BinOp>(boolTy,
                                               "$rangeOR$" + op_name +
                                                 value_name(llvm_i),
                                               *range, *r_ptr, BinOp::Or);
            range = range_or.get();
            BB->addInstr(std::move(range_or));
          } else {
            range = r_ptr;
          }
        }

        if (range)
          BB->addInstr(make_unique<Assume>(*range, Assume::IfNonPoison));
        break;
      }

      case LLVMContext::MD_tbaa:
        // skip this for now
        break;

      // non-relevant for correctness
      case LLVMContext::MD_loop:
      case LLVMContext::MD_prof:
      case LLVMContext::MD_unpredictable:
        break;

      default:
        *out << "ERROR: Unsupported metadata: " << ID << '\n';
        return false;
      }
    }
    return true;
  }

  // If is_callsite is true, encode attributes at call sites' params
  // Otherwise, encode attributes at function definition's arguments
  // TODO: Once attributes at a call site are fully supported, we should
  // remove is_callsite flag
  bool handleParamAttrs(const llvm::AttributeSet &aset, ParamAttrs &attrs,
                        bool is_callsite) {
    bool precise = true;
    for (const llvm::Attribute &llvmattr : aset) {
      // To call getKindAsEnum, llvmattr should be enum or int attribute
      // llvm/include/llvm/IR/Attributes.td has attribute types
      if (!llvmattr.isEnumAttribute() && // e.g. nonnull
          !llvmattr.isIntAttribute() &&  // e.g. dereferenceable
          !llvmattr.isTypeAttribute())   // e.g. byval
        continue;

      switch (llvmattr.getKindAsEnum()) {
      case llvm::Attribute::InReg:
      case llvm::Attribute::SExt:
      case llvm::Attribute::ZExt:
        // TODO: not important for IR verification, but we should check that
        // they don't change
        continue;

      case llvm::Attribute::ByVal: {
        attrs.set(ParamAttrs::ByVal);
        auto ty = aset.getByValType();
        auto asz = DL().getTypeAllocSize(ty);
        attrs.blockSize = max(attrs.blockSize, asz.getKnownMinSize());

        attrs.set(ParamAttrs::Align);
        attrs.align = max(attrs.align, DL().getABITypeAlignment(ty));
        continue;
      }

      case llvm::Attribute::NonNull:
        attrs.set(ParamAttrs::NonNull);
        continue;

      case llvm::Attribute::NoCapture:
        attrs.set(ParamAttrs::NoCapture);
        continue;

      case llvm::Attribute::ReadOnly:
        if (!is_callsite)
          attrs.set(ParamAttrs::NoWrite);
        continue;

      case llvm::Attribute::WriteOnly:
        if (!is_callsite)
          attrs.set(ParamAttrs::NoRead);
        continue;

      case llvm::Attribute::ReadNone:
        if (!is_callsite) {
          // TODO: can this pointer be freed?
          attrs.set(ParamAttrs::NoRead);
          attrs.set(ParamAttrs::NoWrite);
        }
        continue;

      case llvm::Attribute::Dereferenceable:
        attrs.set(ParamAttrs::Dereferenceable);
        attrs.derefBytes = max(attrs.derefBytes,
                               llvmattr.getDereferenceableBytes());
        continue;

      case llvm::Attribute::DereferenceableOrNull:
        attrs.set(ParamAttrs::DereferenceableOrNull);
        attrs.derefOrNullBytes = max(attrs.derefOrNullBytes,
                                     llvmattr.getDereferenceableOrNullBytes());
        continue;

      case llvm::Attribute::Alignment:
        attrs.set(ParamAttrs::Align);
        attrs.align = max(attrs.align, llvmattr.getAlignment()->value());
        continue;

      case llvm::Attribute::NoUndef:
        attrs.set(ParamAttrs::NoUndef);
        continue;

      case llvm::Attribute::Returned:
        attrs.set(ParamAttrs::Returned);
        continue;

      default:
        // If it is call site, it should be added at approximation list
        if (!is_callsite)
          errorAttr(llvmattr);
        precise = false;
      }
    }
    return precise;
  }

  void handleRetAttrs(const llvm::AttributeSet &aset, FnAttrs &attrs) {
    for (const llvm::Attribute &llvmattr : aset) {
      if (!llvmattr.isEnumAttribute() && !llvmattr.isIntAttribute() &&
          !llvmattr.isTypeAttribute())
        continue;

      switch (llvmattr.getKindAsEnum()) {
      case llvm::Attribute::NoAlias: attrs.set(FnAttrs::NoAlias); break;
      case llvm::Attribute::NonNull: attrs.set(FnAttrs::NonNull); break;
      case llvm::Attribute::NoUndef: attrs.set(FnAttrs::NoUndef); break;

      case llvm::Attribute::Dereferenceable:
        attrs.set(FnAttrs::Dereferenceable);
        attrs.derefBytes = max(attrs.derefBytes,
                              llvmattr.getDereferenceableBytes());
        break;
      case llvm::Attribute::DereferenceableOrNull:
        attrs.set(FnAttrs::DereferenceableOrNull);
        attrs.derefOrNullBytes = max(attrs.derefOrNullBytes,
                                     llvmattr.getDereferenceableOrNullBytes());
        break;

      case llvm::Attribute::Alignment:
        attrs.set(FnAttrs::Align);
        attrs.align = max(attrs.align,
                          (unsigned)llvmattr.getAlignment()->value());
        break;

      default: break;
      }
    }
  }

  void handleFnAttrs(const llvm::AttributeSet &aset, FnAttrs &attrs) {
    for (const llvm::Attribute &llvmattr : aset) {
      if (!llvmattr.isEnumAttribute() && !llvmattr.isIntAttribute() &&
          !llvmattr.isTypeAttribute())
        continue;

      switch (llvmattr.getKindAsEnum()) {
      case llvm::Attribute::ReadOnly:
        attrs.set(FnAttrs::NoWrite);
        attrs.set(FnAttrs::NoFree);
        break;

      case llvm::Attribute::ReadNone:
        attrs.set(FnAttrs::NoRead);
        attrs.set(FnAttrs::NoWrite);
        attrs.set(FnAttrs::NoFree);
        break;

      case llvm::Attribute::WriteOnly:
        attrs.set(FnAttrs::NoRead);
        break;

      case llvm::Attribute::ArgMemOnly:
        attrs.set(FnAttrs::ArgMemOnly);
        break;

      case llvm::Attribute::InaccessibleMemOnly:
        attrs.set(FnAttrs::InaccessibleMemOnly);
        break;

      case llvm::Attribute::NoFree:
        attrs.set(FnAttrs::NoFree);
        break;

      case llvm::Attribute::NoReturn:
        attrs.set(FnAttrs::NoReturn);
        break;

      case llvm::Attribute::WillReturn:
        attrs.set(FnAttrs::WillReturn);
        break;

      default:
        break;
      }
    }
  }

  void parse_fn_attrs(const llvm::CallInst &i, FnAttrs &attrs) {
    auto fn = i.getCalledFunction();
    llvm::AttributeList attrs_callsite = i.getAttributes();
    llvm::AttributeList attrs_fndef = fn ? fn->getAttributes()
                                          : llvm::AttributeList();
    auto ret = llvm::AttributeList::ReturnIndex;
    auto fnidx = llvm::AttributeList::FunctionIndex;

    handleRetAttrs(attrs_callsite.getAttributes(ret), attrs);
    handleRetAttrs(attrs_fndef.getAttributes(ret), attrs);
    handleFnAttrs(attrs_callsite.getAttributes(fnidx), attrs);
    handleFnAttrs(attrs_fndef.getAttributes(fnidx), attrs);
  }


  optional<Function> run() {
    constexpr_idx = 0;
    copy_idx = 0;
    alignopbundle_idx = 0;

    // don't even bother if number of BBs or instructions is huge..
    if (distance(f.begin(), f.end()) > 5000 ||
        f.getInstructionCount() > 10000) {
      *out << "ERROR: Function is too large\n";
      return {};
    }

    auto type = llvm_type2alive(f.getReturnType());
    if (!type)
      return {};

    Function Fn(*type, f.getName().str(), 8 * DL().getPointerSize(),
                DL().getIndexSizeInBits(0), DL().isLittleEndian(),
                f.isVarArg());
    reset_state(Fn);

    llvm::AttributeList attrlist = f.getAttributes();

    for (unsigned idx = 0; idx < f.arg_size(); ++idx) {
      llvm::Argument &arg = *f.getArg(idx);
      llvm::AttributeSet argattr =
          attrlist.getAttributes(llvm::AttributeList::FirstArgIndex + idx);

      auto ty = llvm_type2alive(arg.getType());
      ParamAttrs attrs;
      if (!ty || !handleParamAttrs(argattr, attrs, false))
        return {};
      auto val = make_unique<Input>(*ty, value_name(arg), std::move(attrs));
      add_identifier(arg, *val.get());

      if (arg.hasReturnedAttr()) {
        // Cache this to avoid linear lookup at return
        assert(Fn.getReturnedInput() == nullptr);
        Fn.setReturnedInput(val.get());
      }
      Fn.addInput(std::move(val));
    }

    auto &attrs = Fn.getFnAttrs();
    const auto &ridx = llvm::AttributeList::ReturnIndex;
    const auto &fnidx = llvm::AttributeList::FunctionIndex;
    handleRetAttrs(attrlist.getAttributes(ridx), attrs);
    handleFnAttrs(attrlist.getAttributes(fnidx), attrs);

    // create all BBs upfront in topological order
    vector<pair<BasicBlock*, llvm::BasicBlock*>> sorted_bbs;
    {
      edgesTy edges;
      vector<llvm::BasicBlock*> bbs;
      unordered_map<llvm::BasicBlock*, unsigned> bb_map;

      auto bb_num = [&](llvm::BasicBlock *bb) {
        auto [I, inserted] = bb_map.emplace(bb, bbs.size());
        if (inserted) {
          bbs.emplace_back(bb);
          edges.emplace_back();
        }
        return I->second;
      };

      for (auto &bb : f) {
        auto n = bb_num(&bb);
        for (auto *dst : llvm::successors(&bb)) {
          auto n_dst = bb_num(dst);
          edges[n].emplace(n_dst);
        }
      }

      for (auto v : top_sort(edges)) {
        sorted_bbs.emplace_back(&Fn.getBB(value_name(*bbs[v])), bbs[v]);
      }
    }

    for (auto &[alive_bb, llvm_bb] : sorted_bbs) {
      BB = alive_bb;
      for (auto &i : *llvm_bb) {
        if (auto I = visit(i)) {
          auto alive_i = I.get();
          BB->addInstr(std::move(I));

          if (i.hasMetadataOtherThanDebugLoc() &&
              !handleMetadata(i, *alive_i))
            return {};
        } else
          return {};
      }
    }

    // BB -> BB edge
    set<pair<llvm::BasicBlock*, llvm::BasicBlock*>> split_edges;
    for (auto &[phi, i] : todo_phis) {
      for (unsigned idx = 0, e = i->getNumIncomingValues(); idx != e; ++idx) {
        if (has_constant_expr(i->getIncomingValue(idx))) {
          split_edges.emplace(i->getParent(), i->getIncomingBlock(idx));
          break;
        }
      }
    }

    auto predecessor = [&](llvm::PHINode *phi, unsigned idx) {
      auto pred = phi->getIncomingBlock(idx);
      auto name = value_name(*pred);
      if (!split_edges.count({phi->getParent(), pred}))
        return name;
      return std::move(name) + "_" +  getBB(phi->getParent()).getName();
    };

    // patch phi nodes for recursive defs
    for (auto &[phi, i] : todo_phis) {
      for (unsigned idx = 0, e = i->getNumIncomingValues(); idx != e; ++idx) {
        // evaluation of constexprs in phi nodes is done "in the edge", thus we
        // introduce a new BB even if not always needed.
        auto val = i->getIncomingValue(idx);
        if (has_constant_expr(val)) {
          auto bridge  = predecessor(i, idx);
          auto &pred   = getBB(i->getIncomingBlock(idx));
          BB = &Fn.insertBBAfter(bridge, pred);

          auto &phi_bb = getBB(i->getParent());
          pred.replaceTargetWith(&phi_bb, BB);
          if (auto op = get_operand(val)) {
            phi->addValue(*op, std::move(bridge));
            BB->addInstr(make_unique<Branch>(phi_bb));
            continue;
          }
          error(*i);
          return {};
        }

        if (auto op = get_operand(val)) {
          phi->addValue(*op, predecessor(i, idx));
        } else {
          error(*i);
          return {};
        }
      }
    }

    auto getGlobalVariable =
      [this](const string &name) -> llvm::GlobalVariable* {
      auto M = f.getParent();
      // If name is a numeric value, the result should be manually found
      const char *chrs = name.data();
      char *end_ptr;
      auto numeric_id = strtoul(chrs, &end_ptr, 10);

      if ((unsigned)(end_ptr - chrs) != (unsigned)name.size())
        return M->getGlobalVariable(name, true);
      else {
        auto itr = M->global_begin(), end = M->global_end();
        for (; itr != end; ++itr) {
          if (itr->hasName())
            continue;
          if (!numeric_id)
            return &(*itr);
          numeric_id--;
        }
        return nullptr;
      }
    };

    // If there is a global variable with initializer, put them at init block.
    auto &entry_name = Fn.getFirstBB().getName();
    BB = &Fn.getBB("#init", true);

    for (auto &gvname : gvnamesInSrc) {
      auto gv = getGlobalVariable(string(gvname));
      if (!gv) {
        // global variable removed or renamed
        *out << "ERROR: Unsupported interprocedural transformation\n";
        return {};
      }
      // If gvname already exists in tgt, get_operand will immediately return
      get_operand(gv);
    }

    map<string, unique_ptr<Store>> stores;
    // Converting initializer may add new global variables to Fn
    // (Fn.numConstants() increases in that case)
    for (unsigned i = 0; i != Fn.numConstants(); ++i) {
      auto GV = dynamic_cast<GlobalVariable *>(&Fn.getConstant(i));
      if (!GV)
        continue;
      auto gv = getGlobalVariable(GV->getName().substr(1));
      if (!gv->isConstant() || !gv->hasDefinitiveInitializer())
        continue;

      auto storedval = get_operand(gv->getInitializer());
      if (!storedval) {
        *out << "ERROR: Unsupported constant: " << *gv->getInitializer()
             << '\n';
        return {};
      }

      stores.emplace(GV->getName(),
                     make_unique<Store>(*GV, *storedval, GV->getAlignment()));
    }

    for (auto &itm : stores)
      // Insert stores in lexicographical order of global var's names
      BB->addInstr(std::move(itm.second));

    if (BB->empty())
      Fn.removeBB(*BB);
    else
      BB->addInstr(make_unique<Branch>(Fn.getBB(entry_name)));

    return std::move(Fn);
  }
};
}

namespace llvm_util {

initializer::initializer(ostream &os, const llvm::DataLayout &DL) {
  init_llvm_utils(os, DL);
}

optional<IR::Function> llvm2alive(llvm::Function &F,
                                  const llvm::TargetLibraryInfo &TLI,
                                  const vector<string_view> &gvnamesInSrc) {
  return llvm2alive_(F, TLI, gvnamesInSrc).run();
}

}
