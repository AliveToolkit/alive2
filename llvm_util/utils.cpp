// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "llvm_util/utils.h"
#include "ir/constant.h"
#include "ir/function.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Operator.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/SourceMgr.h"
#include <unordered_map>
#include <utility>
#include <vector>

using namespace IR;
using namespace std;
using llvm::cast, llvm::dyn_cast, llvm::isa;

namespace {

// cache Value*'s names
unordered_map<const llvm::Value*, string> value_names;
unsigned value_id_counter = 0; // for %0, %1, etc..

vector<unique_ptr<IntType>> int_types;
vector<unique_ptr<PtrType>> ptr_types;
FloatType half_type("half", FloatType::Half);
FloatType float_type("float", FloatType::Float);
FloatType double_type("double", FloatType::Double);
FloatType quad_type("fp128", FloatType::Quad);
FloatType bfloat_type("bfloat", FloatType::BFloat);

// cache complex types
unordered_map<const llvm::Type*, unique_ptr<Type>> type_cache;
unsigned type_id_counter; // for unnamed types

Function *current_fn;
unordered_map<const llvm::Value*, Value*> value_cache;

ostream *out;

const llvm::DataLayout *DL;

bool hasOpaqueType(llvm::Type *ty) {
  if (auto aty = llvm::dyn_cast<llvm::StructType>(ty)) {
    if (aty->isOpaque())
      return true;

    for (auto elemty : aty->elements())
      if (hasOpaqueType(elemty))
        return true;
  } else if (auto aty = llvm::dyn_cast<llvm::ArrayType>(ty))
    return hasOpaqueType(aty->getElementType());
  else if (auto vty = llvm::dyn_cast<llvm::VectorType>(ty))
    return hasOpaqueType(vty->getElementType());

  return false;
}

}

namespace llvm_util {

FastMathFlags parse_fmath(llvm::Instruction &i) {
  FastMathFlags fmath;
  if (auto op = dyn_cast<llvm::FPMathOperator>(&i)) {
    if (op->hasNoNaNs())
      fmath.flags |= FastMathFlags::NNaN;
    if (op->hasNoInfs())
      fmath.flags |= FastMathFlags::NInf;
    if (op->hasNoSignedZeros())
      fmath.flags |= FastMathFlags::NSZ;
    if (op->hasAllowReciprocal())
      fmath.flags |= FastMathFlags::ARCP;
    if (op->hasAllowContract())
      fmath.flags |= FastMathFlags::Contract;
    if (op->hasAllowReassoc())
      fmath.flags |= FastMathFlags::Reassoc;
    if (op->hasApproxFunc())
      fmath.flags |= FastMathFlags::AFN;
  }
  return fmath;
}

BasicBlock& getBB(const llvm::BasicBlock *bb) {
  return current_fn->getBB(value_name(*bb));
}

string value_name(const llvm::Value &v) {
  auto &name = value_names[&v];
  if (!name.empty())
    return name;

  if (!v.getName().empty())
    return name = '%' + v.getName().str();
  return name = v.getType()->isVoidTy() ? "<void>"
                                        : "%#" + to_string(value_id_counter++);
}

Type& get_int_type(unsigned bits) {
  if (bits >= int_types.size())
    int_types.resize(bits + 1);
  if (!int_types[bits])
    int_types[bits] = make_unique<IntType>("i" + to_string(bits), bits);
  return *int_types[bits].get();
}

Type* llvm_type2alive(const llvm::Type *ty) {
  switch (ty->getTypeID()) {
  case llvm::Type::VoidTyID:
    return &Type::voidTy;
  case llvm::Type::IntegerTyID:
    return &get_int_type(cast<llvm::IntegerType>(ty)->getBitWidth());
  case llvm::Type::HalfTyID:
    return &half_type;
  case llvm::Type::FloatTyID:
    return &float_type;
  case llvm::Type::DoubleTyID:
    return &double_type;
  case llvm::Type::FP128TyID:
    return &quad_type;
  case llvm::Type::BFloatTyID:
    return &bfloat_type;

  case llvm::Type::PointerTyID: {
    // TODO: support for non-64 bits pointers
    unsigned as = cast<llvm::PointerType>(ty)->getAddressSpace();
    // TODO: add support for non-0 AS
    if (as != 0)
      return nullptr;

    if (as >= ptr_types.size())
      ptr_types.resize(as + 1);
    if (!ptr_types[as])
      ptr_types[as] = make_unique<PtrType>(as);
    return ptr_types[as].get();
  }

  case llvm::Type::FunctionTyID:
    return ptr_types[0].get();

  case llvm::Type::StructTyID: {
    auto strty = cast<llvm::StructType>(ty);
    // 8 bits should be plenty to represent all unique values of this type
    // in the program
    if (strty->isOpaque())
      return &get_int_type(8);

    auto &cache = type_cache[ty];
    if (!cache) {
      vector<Type*> elems;
      vector<bool> is_padding;
      auto layout = DL->getStructLayout(const_cast<llvm::StructType *>(strty));
      for (unsigned i = 0; i < strty->getNumElements(); ++i) {
        auto e = strty->getElementType(i);
        auto ofs = layout->getElementOffset(i);
        auto sz = DL->getTypeStoreSize(e);

        // TODO: support vscale
        if (ofs.isScalable() || sz.isScalable())
          return nullptr;

        if (auto ty = llvm_type2alive(e)) {
          elems.push_back(ty);
          is_padding.push_back(false);
        } else
          return nullptr;

        auto ofs_next = i + 1 == strty->getNumElements() ?
                DL->getTypeAllocSize(const_cast<llvm::StructType *>(strty)) :
                layout->getElementOffset(i + 1);
        // TODO: support vscale
        if (ofs_next.isScalable())
          return nullptr;
        assert(ofs + sz <= ofs_next);

        if (ofs_next != ofs + sz) {
          unsigned padsz = 8 * (ofs_next - ofs - sz);
          auto padding_ty = llvm::IntegerType::get(strty->getContext(), padsz);
          if (auto ty = llvm_type2alive(padding_ty)) {
            elems.push_back(ty);
            is_padding.push_back(true);
          } else
            return nullptr;
        }
      }
      cache = make_unique<StructType>("ty_" + to_string(type_id_counter++),
                                      std::move(elems), std::move(is_padding));
    }
    return cache.get();
  }
  // TODO: non-fixed sized vectors
  case llvm::Type::FixedVectorTyID:
  case llvm::Type::ScalableVectorTyID: {
    auto &cache = type_cache[ty];
    if (!cache) {
      auto vty = cast<llvm::VectorType>(ty);
      auto elems = vty->getElementCount().getKnownMinValue();
      auto ety = llvm_type2alive(vty->getElementType());
      if (!ety || elems > 1024)
        return nullptr;
      cache = make_unique<VectorType>("ty_" + to_string(type_id_counter++),
                                      elems, *ety, vty->isScalableTy());
    }
    return cache.get();
  }
  case llvm::Type::ArrayTyID: {
    auto &cache = type_cache[ty];
    if (!cache) {
      auto aty = cast<llvm::ArrayType>(ty);
      auto elemty = aty->getElementType();
      auto elems = aty->getNumElements();
      auto ety = llvm_type2alive(elemty);
      if (!ety || elems > 100 * 1024)
        return nullptr;

      auto sz_with_padding = DL->getTypeAllocSize(elemty);
      auto sz = DL->getTypeStoreSize(elemty);
      assert(DL->getTypeAllocSize(const_cast<llvm::ArrayType *>(aty)) ==
             elems * sz_with_padding);
      Type *paddingTy = sz == sz_with_padding ? 0 :
          llvm_type2alive(llvm::IntegerType::get(aty->getContext(),
                                                 8 * (sz_with_padding - sz)));
      cache = make_unique<ArrayType>("ty_" + to_string(type_id_counter++),
                                     elems, *ety, paddingTy);
    }
    return cache.get();
  }
  default:
    *out << "ERROR: Unsupported type: " << *ty << '\n';
    return nullptr;
  }
}


Value* make_intconst(uint64_t val, int bits) {
  auto c = make_unique<IntConst>(get_int_type(bits), val);
  auto ret = c.get();
  current_fn->addConstant(std::move(c));
  return ret;
}

IR::Value* make_intconst(const llvm::APInt &val) {
  unique_ptr<IntConst> c;
  auto bw = val.getBitWidth();
  auto &ty = get_int_type(bw);
  if (bw <= 64)
    c = make_unique<IntConst>(ty, val.getZExtValue());
  else
    c = make_unique<IntConst>(ty, toString(val, 10, false));
  auto ret = c.get();
  current_fn->addConstant(std::move(c));
  return ret;
}

IR::Value* get_poison(Type &ty) {
  auto val = make_unique<PoisonValue>(ty);
  auto ret = val.get();
  current_fn->addConstant(std::move(val));
  return ret;
}

#define RETURN_CACHE(val)                           \
  do {                                              \
    auto val_cpy = val;                             \
    ENSURE(value_cache.emplace(v, val_cpy).second); \
    return val_cpy;                                 \
  } while (0)


Value* get_operand(llvm::Value *v,
                   function<Value*(llvm::ConstantExpr*)> constexpr_conv,
                   function<Value*(AggregateValue*)> copy_inserter,
                   function<bool(llvm::Function*)> register_fn_decl) {
  if (auto ptr = get_identifier(*v))
    return ptr;

  auto ty = llvm_type2alive(v->getType());
  if (!ty)
    return nullptr;

  // automatic splat of constant values
  if (auto vty = dyn_cast<llvm::FixedVectorType>(v->getType());
      vty && isa<llvm::ConstantInt, llvm::ConstantFP>(v)) {
    llvm::Value *llvm_splat = nullptr;
    if (auto cnst = dyn_cast<llvm::ConstantInt>(v)) {
      llvm_splat
        = llvm::ConstantInt::get(vty->getElementType(), cnst->getValue());
    } else if (auto cnst = dyn_cast<llvm::ConstantFP>(v)) {
      llvm_splat
        = llvm::ConstantFP::get(vty->getElementType(), cnst->getValue());
    } else
      UNREACHABLE();

    auto splat = get_operand(llvm_splat, constexpr_conv, copy_inserter,
                             register_fn_decl);
    if (!splat)
      return nullptr;

    vector<Value*> vals(vty->getNumElements(), splat);
    auto val = make_unique<AggregateValue>(*ty, std::move(vals));
    auto ret = val.get();
    current_fn->addConstant(std::move(val));
    RETURN_CACHE(ret);
  }

  if (auto cnst = dyn_cast<llvm::ConstantInt>(v)) {
    RETURN_CACHE(make_intconst(cnst->getValue()));
  }

  if (auto cnst = dyn_cast<llvm::ConstantFP>(v)) {
    auto &apfloat = cnst->getValueAPF();
    auto c
     = make_unique<FloatConst>(*ty,
                               toString(apfloat.bitcastToAPInt(), 10, false),
                               true);
    auto ret = c.get();
    current_fn->addConstant(std::move(c));
    RETURN_CACHE(ret);
  }

  if (isa<llvm::PoisonValue>(v)) {
    RETURN_CACHE(get_poison(*ty));
  }

  if (isa<llvm::UndefValue>(v)) {
    auto val = make_unique<UndefValue>(*ty);
    auto ret = val.get();
    current_fn->addUndef(std::move(val));
    RETURN_CACHE(ret);
  }

  if (isa<llvm::ConstantPointerNull>(v)) {
    auto val = make_unique<NullPointerValue>(*ty);
    auto ret = val.get();
    current_fn->addConstant(std::move(val));
    RETURN_CACHE(ret);
  }

  if (auto gv = dyn_cast<llvm::GlobalVariable>(v)) {
    if (hasOpaqueType(gv->getValueType()))
      // TODO: Global variable of opaque type is not supported.
      return nullptr;

    unsigned size = DL->getTypeAllocSize(gv->getValueType());
    unsigned align = gv->getPointerAlignment(*DL).value();
    string name;
    if (!gv->hasName()) {
      unsigned id = 0;
      auto M = gv->getParent();
      auto i = M->global_begin(), e = M->global_end();
      for (; i != e; ++i) {
        if (i->hasName())
          continue;
        if (&(*i) == gv)
          break;
        ++id;
      }
      assert(i != e);
      name = '@' + to_string(id);
    } else {
      name = '@' + gv->getName().str();
    }

    bool arb_size = false;
    if (auto *arr = dyn_cast<llvm::ArrayType>(gv->getValueType()))
      arb_size = arr->getNumElements() == 0;

    auto val = make_unique<GlobalVariable>(*ty, std::move(name), size, align,
                                           gv->isConstant(), arb_size);
    auto gvar = val.get();
    current_fn->addConstant(std::move(val));
    RETURN_CACHE(gvar);
  }

  if (auto fn = dyn_cast<llvm::Function>(v)) {
    auto val = make_unique<GlobalVariable>(
      *ty, '@' + fn->getName().str(), 0,
      fn->getAlign().value_or(llvm::Align(8)).value(), true, true, true);
    auto gvar = val.get();
    current_fn->addConstant(std::move(val));

    if (!register_fn_decl(fn))
      return nullptr;

    RETURN_CACHE(gvar);
  }

  auto fillAggregateValues = [&](AggregateType *aty,
      function<llvm::Value *(unsigned)> get_elem, vector<Value*> &vals) -> bool
  {
    unsigned opi = 0;

    for (unsigned i = 0; i < aty->numElementsConst(); ++i) {
      if (!aty->isPadding(i)) {
        if (auto op = get_operand(get_elem(opi), constexpr_conv, copy_inserter,
                                  register_fn_decl))
          vals.emplace_back(op);
        else
          return false;
        ++opi;
      }
    }
    return true;
  };

  if (auto cnst = dyn_cast<llvm::ConstantAggregate>(v)) {
    vector<Value*> vals;
    if (!fillAggregateValues(dynamic_cast<AggregateType *>(ty),
            [&cnst](auto i) { return cnst->getOperand(i); }, vals))
      return nullptr;

    auto val = make_unique<AggregateValue>(*ty, std::move(vals));
    auto ret = val.get();
    if (all_of(cnst->op_begin(), cnst->op_end(), [](auto &V) -> bool
        { return isa<llvm::ConstantData>(V); })) {
      current_fn->addConstant(std::move(val));
      RETURN_CACHE(ret);
    } else {
      current_fn->addAggregate(std::move(val));
      return copy_inserter(ret);
    }
  }

  if (auto cnst = dyn_cast<llvm::ConstantDataSequential>(v)) {
    vector<Value*> vals;
    if (!fillAggregateValues(dynamic_cast<AggregateType *>(ty),
            [&cnst](auto i) { return cnst->getElementAsConstant(i); }, vals))
      return nullptr;

    auto val = make_unique<AggregateValue>(*ty, std::move(vals));
    auto ret = val.get();
    current_fn->addConstant(std::move(val));
    RETURN_CACHE(ret);
  }

  if (auto cnst = dyn_cast<llvm::ConstantAggregateZero>(v)) {
    vector<Value*> vals;
    if (!fillAggregateValues(dynamic_cast<AggregateType *>(ty),
            [&cnst](auto i) { return cnst->getElementValue(i); }, vals))
      return nullptr;

    auto val = make_unique<AggregateValue>(*ty, std::move(vals));
    auto ret = val.get();
    current_fn->addConstant(std::move(val));
    RETURN_CACHE(ret);
  }

  if (auto cexpr = dyn_cast<llvm::ConstantExpr>(v)) {
    return constexpr_conv(cexpr);
  }

  // This must be an operand of an unreachable instruction as the operand
  // hasn't been seen yet
  if (isa<llvm::Instruction>(v)) {
    auto val = make_unique<PoisonValue>(*ty);
    auto ret = val.get();
    current_fn->addConstant(std::move(val));
    return ret;
  }

  return nullptr;
}


void add_identifier(const llvm::Value &llvm, Value &v) {
  value_cache.emplace(&llvm, &v);
}

void replace_identifier(const llvm::Value &llvm, Value &v) {
  value_cache[&llvm] =  &v;
}

Value* get_identifier(const llvm::Value &llvm) {
  auto I = value_cache.find(&llvm);
  return I != value_cache.end() ? I->second : nullptr;
}


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


void init_llvm_utils(ostream &os, const llvm::DataLayout &dataLayout) {
  out = &os;
  type_id_counter = 0;
  int_types.resize(65);
  int_types[1] = make_unique<IntType>("i1", 1);
  ptr_types.emplace_back(make_unique<PtrType>(0));
  DL = &dataLayout;
}

ostream& get_outs() {
  return *out;
}

void set_outs(ostream &os) {
  out = &os;
}

void reset_state() {
  value_cache.clear();
  value_names.clear();
  value_id_counter = 0;
}

void reset_state(Function &f) {
  current_fn = &f;
}

static llvm::ExitOnError ExitOnErr;

// adapted from llvm-dis.cpp
std::unique_ptr<llvm::Module> openInputFile(llvm::LLVMContext &Context,
                                            const string &InputFilename) {
  auto MB =
    ExitOnErr(errorOrToExpected(llvm::MemoryBuffer::getFile(InputFilename)));
  llvm::SMDiagnostic Diag;
  auto M = getLazyIRModule(std::move(MB), Diag, Context,
                           /*ShouldLazyLoadMetadata=*/true);
  if (!M) {
    Diag.print("", llvm::errs(), false);
    return 0;
  }
  ExitOnErr(M->materializeAll());
  return M;
}

llvm::Function *findFunction(llvm::Module &M, const string &FName) {
  auto F = M.getFunction(FName);
  return F && !F->isDeclaration() ? F : nullptr;
}

TailCallInfo parse_fn_tailcall(const llvm::CallInst &i) {
  bool is_tailcall = i.isTailCall() || i.isMustTailCall();
  if (!is_tailcall)
    return {};
  auto tail_type =
      i.isMustTailCall() ? TailCallInfo::MustTail : TailCallInfo::Tail;
  bool has_same_cc = i.getCallingConv() == i.getCaller()->getCallingConv();
  return {tail_type, has_same_cc};
}
}
