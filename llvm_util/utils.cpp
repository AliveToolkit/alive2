// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "llvm_util/utils.h"
#include "llvm/IR/GetElementPtrTypeIterator.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/IR/Operator.h"
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace IR;
using namespace std;
using llvm::cast, llvm::dyn_cast, llvm::isa;
using llvm::LLVMContext;

namespace {

// cache Value*'s names
unordered_map<const llvm::Value*, string> value_names;
unsigned value_id_counter; // for %0, %1, etc..

vector<unique_ptr<IntType>> int_types;
vector<unique_ptr<PtrType>> ptr_types;
FloatType half_type("half", FloatType::Half);
FloatType float_type("float", FloatType::Float);
FloatType double_type("double", FloatType::Double);

// cache complex types
unordered_map<const llvm::Type*, unique_ptr<Type>> type_cache;
unsigned type_id_counter; // for unamed types

ostream *out;
Function *current_fn;
unordered_map<const llvm::Value*, Value*> identifiers;

#if 0
string_view s(llvm::StringRef str) {
  return { str.data(), str.size() };
}
#endif

#define PRINT(T)                                \
ostream& operator<<(ostream &os, const T &x) {  \
  string str;                                   \
  llvm::raw_string_ostream ss(str);             \
  ss << x;                                      \
  return os << ss.str();                        \
}
PRINT(llvm::Type)
PRINT(llvm::Value)
#undef PRINT


void reset_state() {
  identifiers.clear();
  current_fn = nullptr;
  value_names.clear();
  value_id_counter = 0;
}

string value_name(const llvm::Value &v) {
  auto &name = value_names[&v];
  if (!name.empty())
    return name;

  if (!v.getName().empty())
    return name = '%' + v.getName().str();
  return name = v.getType()->isVoidTy() ? "<void>"
                                        : '%' + to_string(value_id_counter++);
}

Type* make_int_type(unsigned bits) {
  if (bits >= int_types.size())
    int_types.resize(bits + 1);
  if (!int_types[bits])
    int_types[bits] = make_unique<IntType>("i" + to_string(bits), bits);
  return int_types[bits].get();
}

Value* make_intconst(uint64_t val, int bits) {
  auto c = make_unique<IntConst>(*make_int_type(bits), val);
  auto ret = c.get();
  current_fn->addConstant(move(c));
  return ret;
}

Type* llvm_type2alive(const llvm::Type *ty) {
  switch (ty->getTypeID()) {
  case llvm::Type::VoidTyID:
    return &Type::voidTy;
  case llvm::Type::IntegerTyID:
    return make_int_type(cast<llvm::IntegerType>(ty)->getBitWidth());
  case llvm::Type::HalfTyID:
    return &half_type;
  case llvm::Type::FloatTyID:
    return &float_type;
  case llvm::Type::DoubleTyID:
    return &double_type;

  case llvm::Type::PointerTyID: {
    // TODO: support for non-64 bits pointers
    unsigned as = cast<llvm::PointerType>(ty)->getAddressSpace();
    if (as >= ptr_types.size())
      ptr_types.resize(as + 1);
    if (!ptr_types[as])
      ptr_types[as] = make_unique<PtrType>(as);
    return ptr_types[as].get();
  }
  case llvm::Type::StructTyID: {
    auto &cache = type_cache[ty];
    if (!cache) {
      vector<Type*> elems;
      for (auto e : cast<llvm::StructType>(ty)->elements()) {
        if (auto ty = llvm_type2alive(e))
          elems.push_back(ty);
        else
          return nullptr;
      }
      cache = make_unique<StructType>("ty_" + to_string(type_id_counter++),
                                      move(elems));
    }
    return cache.get();
  }
  default:
    *out << "Unsupported type: " << *ty << '\n';
    return nullptr;
  }
}


Value* get_operand(llvm::Value *v) {
  if (isa<llvm::Instruction>(v) || isa<llvm::Argument>(v))
    return identifiers[v];

  auto ty = llvm_type2alive(v->getType());
  if (!ty)
    return nullptr;

  // TODO: cache these?
  if (auto cnst = dyn_cast<llvm::ConstantInt>(v)) {
    unique_ptr<IntConst> c;
    if (cnst->getBitWidth() <= 64)
      c = make_unique<IntConst>(*ty, cnst->getZExtValue());
    else
      c = make_unique<IntConst>(*ty, cnst->getValue().toString(10, false));
    auto ret = c.get();
    current_fn->addConstant(move(c));
    return ret;
  }

  if (auto cnst = dyn_cast<llvm::ConstantFP>(v)) {
    auto &apfloat = cnst->getValueAPF();
    auto apint = apfloat.bitcastToAPInt();
    double val;
    if (apint.getBitWidth() == 32) {
      val = apfloat.convertToFloat();
    } else if (apint.getBitWidth() == 64) {
      val = apfloat.convertToDouble();
    } else // TODO
      return nullptr;

    auto c = make_unique<FloatConst>(*ty, val);
    auto ret = c.get();
    current_fn->addConstant(move(c));
    return ret;
  }

  if (isa<llvm::UndefValue>(v)) {
    auto val = make_unique<UndefValue>(*ty);
    auto ret = val.get();
    current_fn->addUndef(move(val));
    return ret;
  }

  return nullptr;
}

BasicBlock& getBB(const llvm::BasicBlock *bb) {
  return current_fn->getBB(value_name(*bb));
}


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

#define RETURN_IDENTIFIER(op)         \
  auto ret = op;                      \
  identifiers.emplace(&i, ret.get()); \
  return ret

class llvm2alive_ : public llvm::InstVisitor<llvm2alive_, unique_ptr<Instr>> {
  llvm::Function &f;
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

public:
  llvm2alive_(llvm::Function &f) : f(f) {}

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

    // TODO: support FP fast-math stuff
    if (isa<llvm::FPMathOperator>(i) && i.getFastMathFlags().any())
      return error(i);

    RETURN_IDENTIFIER(make_unique<BinOp>(*ty, value_name(i), *a, *b, alive_op,
                                         (BinOp::Flags)flags));
  }

  RetTy visitCastInst(llvm::CastInst &i) {
    PARSE_UNOP();
    ConversionOp::Op op;
    switch (i.getOpcode()) {
    case llvm::Instruction::SExt:     op = ConversionOp::SExt; break;
    case llvm::Instruction::ZExt:     op = ConversionOp::ZExt; break;
    case llvm::Instruction::Trunc:    op = ConversionOp::Trunc; break;
    case llvm::Instruction::BitCast:  op = ConversionOp::BitCast; break;
    case llvm::Instruction::PtrToInt: op = ConversionOp::Ptr2Int; break;
    case llvm::Instruction::IntToPtr: op = ConversionOp::Int2Ptr; break;
    default:
      return error(i);
    }
    RETURN_IDENTIFIER(make_unique<ConversionOp>(*ty, value_name(i), *val, op));
  }

  RetTy visitCallInst(llvm::CallInst &i) {
    // TODO: support attributes
    auto fn = i.getCalledFunction();
    if (!fn) // TODO: support indirect calls
      return error(i);

    auto ty = llvm_type2alive(i.getType());
    if (!ty)
      return error(i);

    string fn_name = '@' + fn->getName().str();
    auto call = make_unique<FnCall>(*ty, value_name(i), move(fn_name));
    for (auto &arg : i.args()) {
      auto a = get_operand(arg);
      if (!a)
        return error(i);
      call->addArg(*a);
    }
    RETURN_IDENTIFIER(move(call));
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
    RETURN_IDENTIFIER(make_unique<ICmp>(*int_types[1].get(), value_name(i),
                                        cond, *a, *b));
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
      auto tru = get_operand(llvm::ConstantInt::getTrue(i.getContext()));
      RETURN_IDENTIFIER(make_unique<UnaryOp>(*int_types[1].get(),
                                             value_name(i), *tru,
                                             UnaryOp::Copy));
    }
    case llvm::CmpInst::FCMP_FALSE: {
      auto fals = get_operand(llvm::ConstantInt::getFalse(i.getContext()));
      RETURN_IDENTIFIER(make_unique<UnaryOp>(*int_types[1].get(),
                                             value_name(i), *fals,
                                             UnaryOp::Copy));
    }
    default:
      UNREACHABLE();
    }

    // TODO: support FP fast-math stuff
    if (i.getFastMathFlags().any())
      return error(i);

    RETURN_IDENTIFIER(make_unique<FCmp>(*int_types[1].get(), value_name(i),
                                        cond, *a, *b));
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

      // TODO: support for struct indexing
      if (I.getStructTypeOrNull())
        return error(i);

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
    PARSE_UNOP();
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

  RetTy visitDbgInfoIntrinsic(llvm::DbgInfoIntrinsic&) { return {}; }
  RetTy visitInstruction(llvm::Instruction &i) { return error(i); }

  RetTy error(llvm::Instruction &i) {
    *out << "Unsupported instruction: " << i << '\n';
    return {};
  }

  bool handleMetadata(llvm::Instruction &llvm_i, Instr &i, BasicBlock &BB) {
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
          auto l = make_unique<ICmp>(*int_types[1].get(),
                                     "$range_l$" + op_name + value_name(llvm_i),
                                     ICmp::SGE, i, *get_operand(low));

          auto h = make_unique<ICmp>(*int_types[1].get(),
                                     "$range_h$" + op_name + value_name(llvm_i),
                                     ICmp::SLT, i, *get_operand(high));

          auto r = make_unique<BinOp>(*int_types[1].get(),
                                      "$range$" + op_name + value_name(llvm_i),
                                      *l.get(), *h.get(), BinOp::And);

          auto r_ptr = r.get();
          BB.addInstr(move(l));
          BB.addInstr(move(h));
          BB.addInstr(move(r));

          if (range) {
            auto range_or = make_unique<BinOp>(*int_types[1].get(),
                                               "$rangeOR$" + op_name +
                                                 value_name(llvm_i),
                                               *range, *r_ptr, BinOp::Or);
            range = range_or.get();
            BB.addInstr(move(range_or));
          } else {
            range = r_ptr;
          }
        }

        if (range)
          BB.addInstr(make_unique<Assume>(*range, true));
        break;
      }
      default:
        *out << "Unsupported metadata: " << ID << '\n';
        return false;
      }
    }
    return true;
  }

  optional<Function> run() {
    reset_state();

    auto type = llvm_type2alive(f.getReturnType());
    if (!type)
      return {};

    Function Fn(*type, f.getName());
    current_fn = &Fn;

    for (auto &arg : f.args()) {
      auto ty = llvm_type2alive(arg.getType());
      if (!ty)
        return {};
      auto val = make_unique<Input>(*ty, value_name(arg));
      identifiers.emplace(&arg, val.get());
      Fn.addInput(move(val));
    }

    // create all BBs upfront to keep LLVM's order
    // FIXME: this can go away once we have CFG analysis
    for (auto &bb : f) {
      Fn.getBB(value_name(bb));
    }

    for (auto &bb : f) {
      auto &BB = Fn.getBB(value_name(bb));
      for (auto &i : bb) {
        if (auto I = visit(i)) {
          auto alive_i = I.get();
          BB.addInstr(move(I));

          if (i.hasMetadataOtherThanDebugLoc() &&
              !handleMetadata(i, *alive_i, BB))
            return {};
        } else
          return {};
      }
    }

    return move(Fn);
  }
};
}

namespace llvm_util {

initializer::initializer(ostream &os) {
  int_types.clear();
  ptr_types.clear();
  type_cache.clear();

  out = &os;
  type_id_counter = 0;
  int_types.resize(65);
  int_types[1] = make_unique<IntType>("i1", 1);
  ptr_types.emplace_back(make_unique<PtrType>(0));
}

optional<IR::Function> llvm2alive(llvm::Function &F) {
  return llvm2alive_(F).run();
}

}
