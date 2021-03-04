// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "llvm_util/llvm2alive.h"
#include "llvm_util/known_fns.h"
#include "llvm_util/utils.h"
#include "util/sort.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/GetElementPtrTypeIterator.h"
#include "llvm/IR/GlobalVariable.h"
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

unsigned constexpr_idx;
unsigned copy_idx;

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


template <typename Fn, typename RetFn>
void parse_fnattrs(FnAttrs &attrs, llvm::Type *retTy, Fn &&hasAttr,
                   RetFn &&hasRetAttr) {
  if (hasAttr(llvm::Attribute::ReadOnly)) {
    attrs.set(FnAttrs::NoWrite);
    attrs.set(FnAttrs::NoFree);
  }
  if (hasAttr(llvm::Attribute::ReadNone)) {
    attrs.set(FnAttrs::NoRead);
    attrs.set(FnAttrs::NoWrite);
    attrs.set(FnAttrs::NoFree);
  }
  if (hasAttr(llvm::Attribute::WriteOnly))
    attrs.set(FnAttrs::NoRead);

  if (hasAttr(llvm::Attribute::ArgMemOnly))
    attrs.set(FnAttrs::ArgMemOnly);

  if (hasAttr(llvm::Attribute::NoFree))
    attrs.set(FnAttrs::NoFree);

  if (hasAttr(llvm::Attribute::NoReturn))
    attrs.set(FnAttrs::NoReturn);

  if (hasRetAttr(llvm::Attribute::NonNull))
    attrs.set(FnAttrs::NonNull);

  if (hasRetAttr(llvm::Attribute::NoUndef))
    attrs.set(FnAttrs::NoUndef);
}


class llvm2alive_ : public llvm::InstVisitor<llvm2alive_, unique_ptr<Instr>> {
  BasicBlock *BB;
  llvm::Function &f;
  const llvm::TargetLibraryInfo &TLI;
  vector<llvm::Instruction*> i_constexprs;
  const vector<string_view> &gvnamesInSrc;
  vector<tuple<Phi*, llvm::PHINode*, unsigned>> todo_phis;
  ostream *out;

  using RetTy = unique_ptr<Instr>;

  auto DL() const { return f.getParent()->getDataLayout(); }

  template <typename T>
  unsigned alignment(T &i, llvm::Type *ty) const {
    auto a = i.getAlignment();
    return a != 0 ? a : DL().getABITypeAlignment(ty);
  }

  template <typename T>
  unsigned pref_alignment(T &i, llvm::Type *ty) const {
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
    BB->addInstr(move(ptr));
    return i;
  }

  Value* copy_inserter(AggregateValue *ag) {
    auto v = make_unique<UnaryOp>(*const_cast<Type *>(&ag->getType()),
                                  "%__copy_" + to_string(copy_idx++), *ag,
                                  UnaryOp::Copy);
    auto val = v.get();
    BB->addInstr(move(v));
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
    UnaryOp::Op op;
    switch (i.getOpcode()) {
    case llvm::Instruction::FNeg: op = UnaryOp::FNeg; break;
    default:
      return error(i);
    }
    RETURN_IDENTIFIER(make_unique<UnaryOp>(*ty, value_name(i), *val, op,
                                           parse_fmath(i)));
  }

  RetTy visitBinaryOperator(llvm::BinaryOperator &i) {
    PARSE_BINOP();
    BinOp::Op alive_op;
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
    case llvm::Instruction::FAdd: alive_op = BinOp::FAdd; break;
    case llvm::Instruction::FSub: alive_op = BinOp::FSub; break;
    case llvm::Instruction::FMul: alive_op = BinOp::FMul; break;
    case llvm::Instruction::FDiv: alive_op = BinOp::FDiv; break;
    case llvm::Instruction::FRem: alive_op = BinOp::FRem; break;
    default:
      return error(i);
    }

    unsigned flags = BinOp::None;
    if (isa<llvm::OverflowingBinaryOperator>(i) && i.hasNoSignedWrap())
      flags |= BinOp::NSW;
    if (isa<llvm::OverflowingBinaryOperator>(i) && i.hasNoUnsignedWrap())
      flags |= BinOp::NUW;
    if (isa<llvm::PossiblyExactOperator>(i) && i.isExact())
      flags = BinOp::Exact;
    RETURN_IDENTIFIER(make_unique<BinOp>(*ty, value_name(i), *a, *b, alive_op,
                                         flags, parse_fmath(i)));
  }

  RetTy visitCastInst(llvm::CastInst &i) {
    PARSE_UNOP();
    ConversionOp::Op op;
    switch (i.getOpcode()) {
    case llvm::Instruction::SExt:     op = ConversionOp::SExt; break;
    case llvm::Instruction::ZExt:     op = ConversionOp::ZExt; break;
    case llvm::Instruction::Trunc:    op = ConversionOp::Trunc; break;
    case llvm::Instruction::BitCast:  op = ConversionOp::BitCast; break;
    case llvm::Instruction::SIToFP:   op = ConversionOp::SIntToFP; break;
    case llvm::Instruction::UIToFP:   op = ConversionOp::UIntToFP; break;
    case llvm::Instruction::FPToSI:   op = ConversionOp::FPToSInt; break;
    case llvm::Instruction::FPToUI:   op = ConversionOp::FPToUInt; break;
    case llvm::Instruction::PtrToInt: op = ConversionOp::Ptr2Int; break;
    case llvm::Instruction::IntToPtr: op = ConversionOp::Int2Ptr; break;
    case llvm::Instruction::FPExt:    op = ConversionOp::FPExt; break;
    case llvm::Instruction::FPTrunc:  op = ConversionOp::FPTrunc; break;
    default:
      return error(i);
    }
    RETURN_IDENTIFIER(make_unique<ConversionOp>(*ty, value_name(i), *val, op));
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
      // call_val, attrs, param_attrs, approx
      auto known = known_call(i, TLI, *BB, args);
      if (get<0>(known))
        RETURN_IDENTIFIER(move(get<0>(known)));

      attrs       = move(get<1>(known));
      param_attrs = move(get<2>(known));
      approx      = get<3>(known);
    }

    auto fn = i.getCalledFunction();
    if (!fn) // TODO: support indirect calls
      return error(i);

    auto ty = llvm_type2alive(i.getType());
    if (!ty)
      return error(i);

    if (fn->getName().substr(0, 15) == "__llvm_profile_")
      return NOP(i);

    parse_fnattrs(attrs, i.getType(),
                  [&i](auto attr) { return i.hasFnAttr(attr); },
                  [&i](auto attr) { return i.hasRetAttr(attr); });

    if (auto op = dyn_cast<llvm::FPMathOperator>(&i)) {
      if (op->hasNoNaNs())
        attrs.set(FnAttrs::NNaN);
    }

    const auto &ret = llvm::AttributeList::ReturnIndex;
    if (uint64_t b = max(i.getDereferenceableBytes(ret),
                         fn->getDereferenceableBytes(ret))) {
      attrs.set(FnAttrs::Dereferenceable);
      attrs.derefBytes = b;
    }

    if (uint64_t b = max(i.getDereferenceableOrNullBytes(ret),
                         fn->getDereferenceableOrNullBytes(ret))) {
      attrs.set(FnAttrs::DereferenceableOrNull);
      attrs.derefOrNullBytes = b;
    }

    {
      uint64_t align = 0;
      llvm::MaybeAlign ra = i.getRetAlign();
      if (ra)
        align = ra->value();

      if (fn->hasAttribute(ret, llvm::Attribute::Alignment))
        align = max(align,
            fn->getAttribute(ret, llvm::Attribute::Alignment)
                .getAlignment()->value());

      if (align) {
        attrs.set(FnAttrs::Align);
        attrs.align = align;
      }
    }

    auto call = make_unique<FnCall>(*ty, value_name(i),
                                    '@' + fn->getName().str(), move(attrs),
                                    approx);
    unique_ptr<Instr> ret_val;

    for (uint64_t argidx = 0, nargs = i.arg_size(); argidx < nargs; ++argidx) {
      auto *arg = args[argidx];
      ParamAttrs attr = argidx < param_attrs.size() ? param_attrs[argidx]
                                                    : ParamAttrs();
      // TODO: Once attributes at a call site are fully supported, we should
      // call handleAttributes().

      if (i.paramHasAttr(argidx, llvm::Attribute::Dereferenceable)) {
        uint64_t derefb = 0;
        // dereferenceable at caller
        if (i.getAttributes()
             .hasParamAttr(argidx, llvm::Attribute::Dereferenceable))
          derefb = i.getParamAttr(argidx, llvm::Attribute::Dereferenceable)
                    .getDereferenceableBytes();
        if (argidx < fn->arg_size())
          derefb = max(derefb,
                       fn->getParamDereferenceableBytes(argidx));
        assert(derefb);
        attr.set(ParamAttrs::Dereferenceable);
        attr.derefBytes = derefb;
      }

      if (i.paramHasAttr(argidx, llvm::Attribute::DereferenceableOrNull)) {
        uint64_t sz = 0;
        // dereferenceable_or_null at caller
        if (i.getAttributes()
             .hasParamAttr(argidx, llvm::Attribute::DereferenceableOrNull))
          sz = i.getParamAttr(argidx, llvm::Attribute::DereferenceableOrNull)
                .getDereferenceableOrNullBytes();
        if (argidx < fn->arg_size())
          sz = max(sz, fn->getParamDereferenceableOrNullBytes(argidx));
        assert(sz);
        attr.set(ParamAttrs::DereferenceableOrNull);
        attr.derefOrNullBytes = sz;
      }

      if (i.paramHasAttr(argidx, llvm::Attribute::Alignment)) {
        uint64_t a = 0;
        // alignment at caller
        if (i.getAttributes()
             .hasParamAttr(argidx, llvm::Attribute::Alignment))
          a = i.getParamAttr(argidx, llvm::Attribute::Alignment)
               .getAlignment()->value();
        if (argidx < fn->arg_size() &&
            fn->hasParamAttribute(argidx, llvm::Attribute::Alignment))
          a = max(a, fn->getParamAlign(argidx)->value());

        assert(a);
        attr.set(ParamAttrs::Align);
        attr.align = a;
      }

      if (i.paramHasAttr(argidx, llvm::Attribute::ByVal)) {
        attr.set(ParamAttrs::ByVal);
        auto ty = i.getParamByValType(argidx);
        attr.blockSize = DL().getTypeAllocSize(ty);
        if (!attr.has(ParamAttrs::Align)) {
          attr.set(ParamAttrs::Align);
          attr.align = DL().getABITypeAlignment(ty);
        }
      }

      if (i.paramHasAttr(argidx, llvm::Attribute::NoCapture))
        attr.set(ParamAttrs::NoCapture);

      if (i.paramHasAttr(argidx, llvm::Attribute::NonNull))
        attr.set(ParamAttrs::NonNull);

      if (i.paramHasAttr(argidx, llvm::Attribute::NoUndef)) {
        if (i.getArgOperand(argidx)->getType()->isAggregateType())
          // TODO: noundef aggregate should be supported; it can have undef
          // padding
          return errorAttr(i.getAttribute(argidx, llvm::Attribute::NoUndef));

        attr.set(ParamAttrs::NoUndef);
      }

      if (i.paramHasAttr(argidx, llvm::Attribute::Returned)) {
        auto call2
          = make_unique<FnCall>(Type::voidTy, "", string(call->getFnName()),
                                FnAttrs(call->getAttributes()), approx);
        for (auto &[arg, flags] : call->getArgs()) {
          call2->addArg(*arg, ParamAttrs(flags));
        }
        call = move(call2);

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

      call->addArg(*arg, move(attr));
    }
    if (ret_val) {
      BB->addInstr(move(call));
      RETURN_IDENTIFIER(move(ret_val));
    }
    RETURN_IDENTIFIER(move(call));
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
    case llvm::CmpInst::FCMP_TRUE: {
      auto tru = get_operand(llvm::ConstantInt::getTrue(i.getType()));
      RETURN_IDENTIFIER(make_unique<UnaryOp>(*ty, value_name(i), *tru,
                                             UnaryOp::Copy));
    }
    case llvm::CmpInst::FCMP_FALSE: {
      auto fals = get_operand(llvm::ConstantInt::getFalse(i.getType()));
      RETURN_IDENTIFIER(make_unique<UnaryOp>(*ty, value_name(i), *fals,
                                             UnaryOp::Copy));
    }
    default:
      UNREACHABLE();
    }
    RETURN_IDENTIFIER(make_unique<FCmp>(*ty, value_name(i), cond, *a, *b,
                                        parse_fmath(i)));
  }

  RetTy visitSelectInst(llvm::SelectInst &i) {
    PARSE_TRIOP();
    RETURN_IDENTIFIER(make_unique<Select>(*ty, value_name(i), *a, *b, *c));
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

    RETURN_IDENTIFIER(move(inst));
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

    RETURN_IDENTIFIER(move(inst));
  }

  bool hasLifetimeStart(llvm::User &i, unordered_set<llvm::Value*> &visited) {
    for (auto I = i.user_begin(), E = i.user_end(); I != E; ++I) {
      llvm::User *U = *I;
      if (!visited.insert(U).second)
        continue;
      if (auto I = llvm::dyn_cast<llvm::IntrinsicInst>(U)) {
        if (I->getIntrinsicID() == llvm::Intrinsic::lifetime_start)
          return true;
      }
      if ((llvm::isa<llvm::BitCastInst>(U) ||
           llvm::isa<llvm::GetElementPtrInst>(U) ||
           llvm::isa<llvm::PHINode>(U)) && hasLifetimeStart(*U, visited))
        return true;
    }
    return false;
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

    unordered_set<llvm::Value*> visited;
    // FIXME: size bits shouldn't be a constant
    auto size = make_intconst(DL().getTypeAllocSize(i.getAllocatedType()), 64);
    RETURN_IDENTIFIER(make_unique<Alloc>(*ty, value_name(i), *size, mul,
                        pref_alignment(i, i.getAllocatedType()),
                        hasLifetimeStart(i, visited)));
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
    RETURN_IDENTIFIER(move(gep));
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

    auto phi = make_unique<Phi>(*ty, value_name(i));
    for (unsigned idx = 0, e = i.getNumIncomingValues(); idx != e; ++idx) {
      if (auto op = get_operand(i.getIncomingValue(idx))) {
        phi->addValue(*op, value_name(*i.getIncomingBlock(idx)));
      } else {
        todo_phis.emplace_back(phi.get(), &i, idx);
      }
    }

    RETURN_IDENTIFIER(move(phi));
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
          if (!aptr || !aalign)
            return error(i);

          vector<Value *> args = {aptr, aalign};
          BB->addInstr(make_unique<Assume>(move(args), Assume::Align));
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
      default:
        UNREACHABLE();
      }
      RETURN_IDENTIFIER(make_unique<BinOp>(*ty, value_name(i), *a, *b, op));
    }
    case llvm::Intrinsic::bitreverse:
    case llvm::Intrinsic::bswap:
    case llvm::Intrinsic::ceil:
    case llvm::Intrinsic::ctpop:
    case llvm::Intrinsic::expect:
    case llvm::Intrinsic::expect_with_probability:
    case llvm::Intrinsic::fabs:
    case llvm::Intrinsic::floor:
    case llvm::Intrinsic::is_constant:
    case llvm::Intrinsic::round:
    case llvm::Intrinsic::roundeven:
    case llvm::Intrinsic::sqrt:
    case llvm::Intrinsic::trunc: {
      PARSE_UNOP();
      UnaryOp::Op op;
      switch (i.getIntrinsicID()) {
      case llvm::Intrinsic::bitreverse:  op = UnaryOp::BitReverse; break;
      case llvm::Intrinsic::bswap:       op = UnaryOp::BSwap; break;
      case llvm::Intrinsic::ceil:        op = UnaryOp::Ceil; break;
      case llvm::Intrinsic::ctpop:       op = UnaryOp::Ctpop; break;
      case llvm::Intrinsic::expect:
      case llvm::Intrinsic::expect_with_probability:
        op = UnaryOp::Copy; break;
      case llvm::Intrinsic::fabs:        op = UnaryOp::FAbs; break;
      case llvm::Intrinsic::floor:       op = UnaryOp::Floor; break;
      case llvm::Intrinsic::is_constant: op = UnaryOp::IsConstant; break;
      case llvm::Intrinsic::round:       op = UnaryOp::Round; break;
      case llvm::Intrinsic::roundeven:   op = UnaryOp::RoundEven; break;
      case llvm::Intrinsic::sqrt:        op = UnaryOp::Sqrt; break;
      case llvm::Intrinsic::trunc:       op = UnaryOp::Trunc; break;
      default: UNREACHABLE();
      }
      RETURN_IDENTIFIER(make_unique<UnaryOp>(*ty, value_name(i), *val, op,
                                             parse_fmath(i)));
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
    case llvm::Intrinsic::fma:
    {
      PARSE_TRIOP();
      TernaryOp::Op op;
      switch (i.getIntrinsicID()) {
      case llvm::Intrinsic::fshl: op = TernaryOp::FShl; break;
      case llvm::Intrinsic::fshr: op = TernaryOp::FShr; break;
      case llvm::Intrinsic::fma:  op = TernaryOp::FMA; break;
      default: UNREACHABLE();
      }
      RETURN_IDENTIFIER(make_unique<TernaryOp>(*ty, value_name(i), *a, *b, *c,
                                               op, parse_fmath(i)));
    }
    case llvm::Intrinsic::minnum:
    case llvm::Intrinsic::maxnum:
    case llvm::Intrinsic::minimum:
    case llvm::Intrinsic::maximum:
    {
      PARSE_BINOP();
      BinOp::Op op;
      switch (i.getIntrinsicID()) {
      case llvm::Intrinsic::minnum:   op = BinOp::FMin; break;
      case llvm::Intrinsic::maxnum:   op = BinOp::FMax; break;
      case llvm::Intrinsic::minimum:  op = BinOp::FMinimum; break;
      case llvm::Intrinsic::maximum:  op = BinOp::FMaximum; break;
      default: UNREACHABLE();
      }
      RETURN_IDENTIFIER(make_unique<BinOp>(*ty, value_name(i), *a, *b,
                                           op, BinOp::None, parse_fmath(i)));
    }
    case llvm::Intrinsic::lifetime_start:
    {
      PARSE_BINOP();
      if (!llvm::isa<llvm::AllocaInst>(llvm::getUnderlyingObject(
          i.getOperand(1))))
        return error(i);
      RETURN_IDENTIFIER(make_unique<StartLifetime>(*b));
    }
    case llvm::Intrinsic::lifetime_end:
    {
      PARSE_BINOP();
      if (!llvm::isa<llvm::AllocaInst>(llvm::getUnderlyingObject(
          i.getOperand(1))))
        return error(i);
      RETURN_IDENTIFIER(make_unique<Free>(*b, false));
    }
    case llvm::Intrinsic::trap:
    {
      FnAttrs attrs;
      attrs.set(FnAttrs::NoReturn);
      attrs.set(FnAttrs::NoWrite);
      return make_unique<FnCall>(*llvm_type2alive(i.getType()),
                                 "", "#trap", move(attrs));
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
                                                 move(mask)));
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
          BB->addInstr(move(l));
          BB->addInstr(move(h));
          BB->addInstr(move(r));

          if (range) {
            auto range_or = make_unique<BinOp>(boolTy,
                                               "$rangeOR$" + op_name +
                                                 value_name(llvm_i),
                                               *range, *r_ptr, BinOp::Or);
            range = range_or.get();
            BB->addInstr(move(range_or));
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

  optional<ParamAttrs> handleAttributes(llvm::Argument &arg) {
    ParamAttrs attrs;
    for (auto &attr : arg.getParent()->getAttributes()
                         .getParamAttributes(arg.getArgNo())) {
      switch (attr.getKindAsEnum()) {
      case llvm::Attribute::InReg:
      case llvm::Attribute::SExt:
      case llvm::Attribute::ZExt:
        // TODO: not important for IR verification, but we should check that
        // they don't change
        continue;

      case llvm::Attribute::ByVal: {
        attrs.set(ParamAttrs::ByVal);
        auto ty = arg.getParamByValType();
        attrs.blockSize = DL().getTypeAllocSize(ty);
        if (!attrs.has(ParamAttrs::Align)) {
          attrs.set(ParamAttrs::Align);
          attrs.align = DL().getABITypeAlignment(ty);
        }
        continue;
      }

      case llvm::Attribute::NonNull:
        attrs.set(ParamAttrs::NonNull);
        continue;

      case llvm::Attribute::NoCapture:
        attrs.set(ParamAttrs::NoCapture);
        continue;

      case llvm::Attribute::ReadOnly:
        attrs.set(ParamAttrs::NoWrite);
        continue;

      case llvm::Attribute::WriteOnly:
        attrs.set(ParamAttrs::NoRead);
        continue;

      case llvm::Attribute::ReadNone:
        // TODO: can this pointer be freed?
        attrs.set(ParamAttrs::NoRead);
        attrs.set(ParamAttrs::NoWrite);
        continue;

      case llvm::Attribute::Dereferenceable:
        attrs.set(ParamAttrs::Dereferenceable);
        attrs.derefBytes = attr.getDereferenceableBytes();
        continue;

      case llvm::Attribute::DereferenceableOrNull:
        attrs.set(ParamAttrs::DereferenceableOrNull);
        attrs.derefOrNullBytes = attr.getDereferenceableOrNullBytes();
        continue;

      case llvm::Attribute::Alignment:
        attrs.set(ParamAttrs::Align);
        attrs.align = attr.getAlignment()->value();
        continue;

      case llvm::Attribute::NoUndef:
        attrs.set(ParamAttrs::NoUndef);
        continue;

      case llvm::Attribute::Returned:
        attrs.set(ParamAttrs::Returned);
        continue;

      default:
        errorAttr(attr);
        return nullopt;
      }
    }
    return attrs;
  }

  optional<Function> run() {
    constexpr_idx = 0;
    copy_idx = 0;

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

    for (auto &arg : f.args()) {
      auto ty = llvm_type2alive(arg.getType());
      auto attrs = handleAttributes(arg);
      if (!ty || !attrs)
        return {};
      auto val = make_unique<Input>(*ty, value_name(arg), move(*attrs));
      add_identifier(arg, *val.get());

      if (arg.hasReturnedAttr()) {
        // Cache this to avoid linear lookup at return
        assert(Fn.getReturnedInput() == nullptr);
        Fn.setReturnedInput(val.get());
      }
      Fn.addInput(move(val));
    }

    auto &attrs = Fn.getFnAttrs();
    const auto &ridx = llvm::AttributeList::ReturnIndex;
    parse_fnattrs(attrs, f.getReturnType(),
                  [&](auto attr) { return f.hasFnAttribute(attr); },
                  [&](auto attr) { return f.hasAttribute(ridx, attr); });

    if (uint64_t b = f.getDereferenceableBytes(ridx)) {
      attrs.set(FnAttrs::Dereferenceable);
      attrs.derefBytes = b;
    }

    if (f.hasAttribute(ridx, llvm::Attribute::Alignment)) {
      unsigned a = f.getAttribute(ridx, llvm::Attribute::Alignment)
                    .getAlignment()->value();
      attrs.set(FnAttrs::Align);
      attrs.align = a;
    }

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
          BB->addInstr(move(I));

          if (i.hasMetadataOtherThanDebugLoc() &&
              !handleMetadata(i, *alive_i))
            return {};
        } else
          return {};
      }
    }

    // patch phi nodes for recursive defs
    for (auto &[phi, llvm_i, idx] : todo_phis) {
      auto op = get_operand(llvm_i->getIncomingValue(idx));
      if (!op) {
        error(*llvm_i);
        return {};
      }
      phi->addValue(*op, value_name(*llvm_i->getIncomingBlock(idx)));
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

      auto *init_ty = gv->getInitializer()->getType();
      if (init_ty->isArrayTy() &&
          init_ty->getArrayNumElements() > omit_array_size)
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
      BB->addInstr(move(itm.second));

    if (BB->empty())
      Fn.removeBB(*BB);
    else
      BB->addInstr(make_unique<Branch>(Fn.getBB(entry_name)));

    return move(Fn);
  }
};
}

namespace llvm_util {

unsigned omit_array_size = -1;


initializer::initializer(ostream &os, const llvm::DataLayout &DL) {
  init_llvm_utils(os, DL);
}

optional<IR::Function> llvm2alive(llvm::Function &F,
                                  const llvm::TargetLibraryInfo &TLI,
                                  const vector<string_view> &gvnamesInSrc) {
  return llvm2alive_(F, TLI, gvnamesInSrc).run();
}

}
