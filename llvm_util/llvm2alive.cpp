// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "llvm_util/llvm2alive.h"
#include "llvm_util/known_fns.h"
#include "llvm_util/utils.h"
#include "llvm/IR/GetElementPtrTypeIterator.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/IR/Operator.h"
#include <utility>
#include <vector>

using namespace llvm_util;
using namespace IR;
using namespace std;
using llvm::cast, llvm::dyn_cast, llvm::isa;
using llvm::LLVMContext;

namespace {

ostream *out;

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
  vector<llvm::Instruction *> i_constexprs;
  vector<string_view> gvnamesInSrc;

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
    static unsigned constexpr_idx = 0;
    newI->setName("__constexpr_" + to_string(constexpr_idx++));
    i_constexprs.push_back(newI);

    auto ptr = this->visit(*newI);
    if (!ptr)
      return nullptr;

    auto i = ptr.get();
    BB->addInstr(move(ptr));
    return i;
  }

  auto get_operand(llvm::Value *v) {
    return llvm_util::get_operand(v,
        [this](auto I) { return convert_constexpr(I); });
  }

public:
  llvm2alive_(llvm::Function &f, const llvm::TargetLibraryInfo &TLI,
              vector<string_view> &&gvnamesInSrc)
      : f(f), TLI(TLI), gvnamesInSrc(move(gvnamesInSrc)) {}

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

    // TODO: support FP fast-math stuff
    if (isa<llvm::FPMathOperator>(i) && i.getFastMathFlags().any())
      return error(i);

    RETURN_IDENTIFIER(make_unique<UnaryOp>(*ty, value_name(i), *val, op));
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

    if (isa<llvm::FPMathOperator>(i)) {
      auto FM = i.getFastMathFlags();
      if (FM.noNaNs())
        flags |= BinOp::NNaN;
      if (FM.noInfs())
        flags |= BinOp::NInf;
      if (FM.noSignedZeros())
        flags |= BinOp::NSZ;
      if (FM.allowReciprocal())
        flags |= BinOp::ARCP;
      if (FM.allowContract())
        flags |= BinOp::Contract;
      if (FM.allowReassoc())
        flags |= BinOp::Reassoc;
    }
    RETURN_IDENTIFIER(make_unique<BinOp>(*ty, value_name(i), *a, *b, alive_op,
                                         flags));
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
    default:
      return error(i);
    }
    RETURN_IDENTIFIER(make_unique<ConversionOp>(*ty, value_name(i), *val, op));
  }

  RetTy visitFreezeInst(llvm::FreezeInst &i) {
    PARSE_UNOP();
    RETURN_IDENTIFIER(make_unique<Freeze>(*ty, value_name(i), *val));
  }

  RetTy visitCallInst(llvm::CallInst &i) {
    auto [call_val, known] = known_call(i, TLI, *BB,
        [this](auto I) { return convert_constexpr(I); });
    if (call_val)
      RETURN_IDENTIFIER(move(call_val));

    // TODO: support attributes
    auto fn = i.getCalledFunction();
    if (!fn) // TODO: support indirect calls
      return error(i);

    auto ty = llvm_type2alive(i.getType());
    if (!ty)
      return error(i);

    string fn_name = '@' + fn->getName().str();
    auto call = make_unique<FnCall>(*ty, value_name(i), move(fn_name), !known);
    for (auto &arg : i.args()) {
      auto a = get_operand(arg);
      if (!a)
        return error(i);
      call->addArg(*a);
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

    unsigned flags = 0;
    if (i.getFastMathFlags().noNaNs())
      flags |= FCmp::NNaN;
    if (i.getFastMathFlags().noInfs())
      flags |= FCmp::NInf;
    if (i.getFastMathFlags().allowReassoc())
      flags |= FCmp::Reassoc;

    RETURN_IDENTIFIER(make_unique<FCmp>(*ty, value_name(i), cond, *a, *b,
                                        flags));
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

    for (auto idx : i.indices()) {
      inst->addIdx(idx);
    }

    RETURN_IDENTIFIER(move(inst));
  }

  RetTy visitAllocaInst(llvm::AllocaInst &i) {
    // TODO
    if (i.isArrayAllocation() || !i.isStaticAlloca())
      return error(i);

    auto ty = llvm_type2alive(i.getType());
    if (!ty)
      return error(i);

    // FIXME: size bits shouldn't be a constant
    auto size = make_intconst(DL().getTypeAllocSize(i.getAllocatedType()), 64);
    RETURN_IDENTIFIER(make_unique<Alloc>(*ty, value_name(i), *size,
                        pref_alignment(i, i.getAllocatedType())));
  }

  RetTy visitGetElementPtrInst(llvm::GetElementPtrInst &i) {
    auto ty = llvm_type2alive(i.getType());
    auto ptr = get_operand(i.getPointerOperand());
    if (!ty || !ptr)
      return error(i);

    auto gep = make_unique<GEP>(*ty, value_name(i), *ptr, i.isInBounds());

    // reference: GEPOperator::accumulateConstantOffset()
    for (auto I = llvm::gep_type_begin(i), E = llvm::gep_type_end(i);
         I != E; ++I) {
      auto op = get_operand(I.getOperand());
      if (!op)
        return error(i);

      if (auto structTy = I.getStructTypeOrNull()) {
        llvm::Value *vals[] = { llvm::ConstantInt::getFalse(i.getContext()),
                                I.getOperand() };
        auto offset = DL().getIndexedOffsetInType(structTy,
                llvm::makeArrayRef(vals, 2));
        gep->addIdx(offset, *make_intconst(1, 64));
        continue;
      }

      gep->addIdx(DL().getTypeAllocSize(I.getIndexedType()), *op);
    }
    RETURN_IDENTIFIER(move(gep));
  }

  RetTy visitLoadInst(llvm::LoadInst &i) {
    // TODO: Add support for isVolatile(), getOrdering()
    PARSE_UNOP();
    RETURN_IDENTIFIER(make_unique<Load>(*ty, value_name(i), *val,
                                        alignment(i, i.getType())));
  }

  RetTy visitStoreInst(llvm::StoreInst &i) {
    // TODO: Add support for isVolatile(), getOrdering()
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
      auto op = get_operand(i.getIncomingValue(idx));
      if (!op)
        return error(i);
      phi->addValue(*op, value_name(*i.getIncomingBlock(idx)));
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
    return make_unique<Assume>(*fals, /*if_non_poison=*/false);
  }

  RetTy visitIntrinsicInst(llvm::IntrinsicInst &i) {
    switch (i.getIntrinsicID()) {
    case llvm::Intrinsic::assume:
    {
      PARSE_UNOP();
      return make_unique<Assume>(*val, false);
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
    case llvm::Intrinsic::cttz:
    case llvm::Intrinsic::ctlz:
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
      case llvm::Intrinsic::cttz:     op = BinOp::Cttz; break;
      case llvm::Intrinsic::ctlz:     op = BinOp::Ctlz; break;
      default: UNREACHABLE();
      }
      RETURN_IDENTIFIER(make_unique<BinOp>(*ty, value_name(i), *a, *b, op));
    }
    case llvm::Intrinsic::bitreverse:
    case llvm::Intrinsic::bswap:
    case llvm::Intrinsic::ctpop:
    case llvm::Intrinsic::expect:
    {
      PARSE_UNOP();
      UnaryOp::Op op;
      switch (i.getIntrinsicID()) {
      case llvm::Intrinsic::bitreverse: op = UnaryOp::BitReverse; break;
      case llvm::Intrinsic::bswap:      op = UnaryOp::BSwap; break;
      case llvm::Intrinsic::ctpop:      op = UnaryOp::Ctpop; break;
      case llvm::Intrinsic::expect:     op = UnaryOp::Copy; break;
      default: UNREACHABLE();
      }
      RETURN_IDENTIFIER(make_unique<UnaryOp>(*ty, value_name(i), *val, op));
    }
    case llvm::Intrinsic::fshl:
    case llvm::Intrinsic::fshr:
    {
      PARSE_TRIOP();
      TernaryOp::Op op;
      switch (i.getIntrinsicID()) {
      case llvm::Intrinsic::fshl: op = TernaryOp::FShl; break;
      case llvm::Intrinsic::fshr: op = TernaryOp::FShr; break;
      default: UNREACHABLE();
      }
      RETURN_IDENTIFIER(make_unique<TernaryOp>(*ty, value_name(i), *a, *b, *c,
                                               op));
    }

    // do nothing intrinsics
    case llvm::Intrinsic::dbg_addr:
    case llvm::Intrinsic::donothing:
      return {};

    default:
      break;
    }
    return error(i);
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
    PARSE_TRIOP();
    RETURN_IDENTIFIER(make_unique<ShuffleVector>(*ty, value_name(i), *a, *b,
                                                 *c));
  }

  RetTy visitDbgInfoIntrinsic(llvm::DbgInfoIntrinsic&) { return {}; }
  RetTy visitInstruction(llvm::Instruction &i) { return error(i); }

  RetTy error(llvm::Instruction &i) {
    *out << "ERROR: Unsupported instruction: " << i << '\n';
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
        for (unsigned op = 0, e = Node->getNumOperands(); op < e; ++op) {
          auto *low =
            llvm::mdconst::extract<llvm::ConstantInt>(Node->getOperand(op));
          auto *high =
            llvm::mdconst::extract<llvm::ConstantInt>(Node->getOperand(++op));

          auto op_name = to_string(op / 2);
          auto l = make_unique<ICmp>(get_int_type(1),
                                     "%range_l#" + op_name + value_name(llvm_i),
                                     ICmp::SGE, i, *get_operand(low));

          auto h = make_unique<ICmp>(get_int_type(1),
                                     "%range_h#" + op_name + value_name(llvm_i),
                                     ICmp::SLT, i, *get_operand(high));

          auto r = make_unique<BinOp>(get_int_type(1),
                                      "%range#" + op_name + value_name(llvm_i),
                                      *l.get(), *h.get(), BinOp::And);

          auto r_ptr = r.get();
          BB->addInstr(move(l));
          BB->addInstr(move(h));
          BB->addInstr(move(r));

          if (range) {
            auto range_or = make_unique<BinOp>(get_int_type(1),
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
          BB->addInstr(make_unique<Assume>(*range, true));
        break;
      }

      case LLVMContext::MD_tbaa:
        // skip this for now
        break;

      // non-relevant for correctness
      case LLVMContext::MD_misexpect:
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

  bool handleAttributes(llvm::Argument &arg) {
    auto attrs = arg.getParent()->getAttributes()
                 .getParamAttributes(arg.getArgNo());
    for (auto &attr : attrs) {
      switch (attr.getKindAsEnum()) {
      default:
        *out << "ERROR: Unsupported attribute: " << attr.getAsString() << '\n';
        return false;
      }
    }
    return true;
  }

  optional<Function> run() {
    auto type = llvm_type2alive(f.getReturnType());
    if (!type)
      return {};

    Function Fn(*type, f.getName(), DL().isLittleEndian());
    reset_state(Fn);

    for (auto &arg : f.args()) {
      auto ty = llvm_type2alive(arg.getType());
      if (!ty)
        return {};
      auto val = make_unique<Input>(*ty, value_name(arg));
      add_identifier(arg, *val.get());
      Fn.addInput(move(val));

      if (!handleAttributes(arg))
        return {};
    }

    // create all BBs upfront to keep LLVM's order
    // FIXME: this can go away once we have CFG analysis
    for (auto &bb : f) {
      Fn.getBB(value_name(bb));
    }

    for (auto &bb : f) {
      BB = &Fn.getBB(value_name(bb));
      for (auto &i : bb) {
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

    // If there is a global variable with initializer, put them at init block.
    auto &entryName = Fn.getFirstBB().getName();
    auto &InitBB = Fn.getBB("__globalvars_init", true);
    BB = &InitBB;
    auto M = f.getParent();
    for (auto &gvname : gvnamesInSrc) {
      auto gv = M->getGlobalVariable(string(gvname), true);
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

      auto gv = M->getGlobalVariable(GV->getName().substr(1), true);
      assert(gv);
      if (!gv->hasInitializer())
        continue;
      else if (auto CE = dyn_cast<llvm::ConstantExpr>(gv->getInitializer())) {
        auto storedval = convert_constexpr(CE);
        auto globalvar = get_operand(gv);
        if (!storedval || !globalvar)
          return {};
        // Alignment is already enforced by Memory::alloc.
        stores.emplace(gv->getName(),
                       make_unique<Store>(*globalvar, *storedval, 1));
      }
    }

    for (auto &itm : stores)
      // Insert stores in lexicographical order of global var's names
      InitBB.addInstr(move(itm.second));
    InitBB.addInstr(make_unique<Branch>(Fn.getBB(entryName)));

    return move(Fn);
  }
};
}

namespace llvm_util {

initializer::initializer(ostream &os, const llvm::DataLayout &DL) {
  out = &os;
  init_llvm_utils(os, DL);
}

optional<IR::Function> llvm2alive(llvm::Function &F,
                                  const llvm::TargetLibraryInfo &TLI,
                                  vector<string_view> &&gvnamesInSrc) {
  return llvm2alive_(F, TLI, move(gvnamesInSrc)).run();
}

}
