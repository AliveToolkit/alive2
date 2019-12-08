// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "llvm_util/utils.h"
#include "ir/constant.h"
#include "ir/function.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"
#include <unordered_map>
#include <utility>
#include <vector>
#include <sstream>

using namespace IR;
using namespace std;
using llvm::cast, llvm::dyn_cast, llvm::isa;

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

Function *current_fn;
unordered_map<const llvm::Value*, Value*> identifiers;
unordered_map<const llvm::GlobalVariable*, GlobalVariable*> globalvars;

ostream *out;

const llvm::DataLayout *DL;

}

namespace llvm_util {

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
                                        : '%' + to_string(value_id_counter++);
}

void remove_value_name(const llvm::Value &v) {
  value_names.erase(&v);
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
      vector<bool> is_padding;
      auto strty = cast<llvm::StructType>(ty);
      auto layout = DL->getStructLayout(const_cast<llvm::StructType *>(strty));
      for (unsigned i = 0; i < strty->getNumElements(); ++i) {
        auto e = strty->getElementType(i);
        unsigned ofs = layout->getElementOffset(i);
        unsigned sz = DL->getTypeStoreSize(e);

        if (auto ty = llvm_type2alive(e)) {
          elems.push_back(ty);
          is_padding.push_back(false);
        } else
          return nullptr;

        unsigned ofs_next = i + 1 == strty->getNumElements() ?
                DL->getTypeAllocSize(const_cast<llvm::StructType *>(strty)) :
                layout->getElementOffset(i + 1);
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
                                      move(elems), move(is_padding));
    }
    return cache.get();
  }
  case llvm::Type::VectorTyID: {
    auto &cache = type_cache[ty];
    if (!cache) {
      auto vty = cast<llvm::VectorType>(ty);
      // TODO: non-fixed sized vectors
      if (vty->isScalable())
        goto err;

      auto elems = vty->getElementCount().Min;
      auto ety = llvm_type2alive(vty->getElementType());
      if (!ety || elems > 128)
        return nullptr;
      cache = make_unique<VectorType>("ty_" + to_string(type_id_counter++),
                                      elems, *ety);
    }
    return cache.get();
  }
  case llvm::Type::ArrayTyID: {
    auto &cache = type_cache[ty];
    if (!cache) {
      auto aty = cast<llvm::ArrayType>(ty);
      auto elems = aty->getNumElements();
      auto ety = llvm_type2alive(aty->getElementType());
      if (!ety || elems > 128)
        return nullptr;
      cache = make_unique<ArrayType>("ty_" + to_string(type_id_counter++),
                                     elems, *ety);
    }
    return cache.get();
  }
  default:
err:
    *out << "ERROR: Unsupported type: " << *ty << '\n';
    return nullptr;
  }
}


Value* make_intconst(uint64_t val, int bits) {
  auto c = make_unique<IntConst>(get_int_type(bits), val);
  auto ret = c.get();
  current_fn->addConstant(move(c));
  return ret;
}


Value* get_operand(llvm::Value *v,
                   function<Value*(llvm::ConstantExpr*)> constexpr_conv) {
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
    unique_ptr<FloatConst> c;
    switch (ty->getAsFloatType()->getFpType()) {
    case FloatType::Half:
      c = make_unique<FloatConst>(*ty,
                                  apfloat.bitcastToAPInt().getLimitedValue());
      break;
    case FloatType::Float:
      c = make_unique<FloatConst>(*ty, apfloat.convertToFloat());
      break;
    case FloatType::Double:
      c = make_unique<FloatConst>(*ty, apfloat.convertToDouble());
      break;
    case FloatType::Unknown:
      UNREACHABLE();
    }
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

  if (isa<llvm::ConstantPointerNull>(v)) {
    auto val = make_unique<NullPointerValue>(*ty);
    auto ret = val.get();
    current_fn->addConstant(move(val));
    return ret;
  }

  if (auto gv = dyn_cast<llvm::GlobalVariable>(v)) {
    auto &gvar = globalvars[gv];
    if (gvar)
      return gvar;

    if (auto st = dyn_cast<llvm::StructType>(gv->getValueType())) {
      if (st->isOpaque())
        // TODO: Global variable of opaque type is not supported.
        return nullptr;
    }

    unsigned size = DL->getTypeAllocSize(gv->getValueType());
    unsigned align = gv->getPointerAlignment(*DL).valueOrOne().value();
    string name;
    if (!gv->hasName()) {
      unsigned id = 0;
      auto M = gv->getParent();
      auto i = M->global_begin(), e = M->global_end();
      for (; i != e; ++i) {
        if (&(*i) == gv)
          break;
        id++;
      }
      assert(i != e);
      stringstream ss;
      ss << '@' << id;
      name = ss.str();
    } else {
      name = '@' + gv->getName().str();
    }
    auto val = make_unique<GlobalVariable>(*ty, move(name), size, align,
                                           gv->isConstant());
    gvar = val.get();
    current_fn->addConstant(move(val));
    return gvar;
  }

  if (auto cnst = dyn_cast<llvm::ConstantAggregate>(v)) {
    vector<Value*> vals;
    auto aty = dynamic_cast<AggregateType *>(ty);
    unsigned opi = 0;

    for (unsigned i = 0; i < aty->numElementsConst(); ++i) {
      if (aty->isPadding(i)) {
        auto &padty = aty->getChild(i);
        assert(padty.isIntType());
        auto poison = make_unique<PoisonValue>(padty);
        auto ret = poison.get();

        current_fn->addConstant(move(poison));
        vals.emplace_back(ret);
      } else {
        if (auto op = get_operand(cnst->getOperand(opi), constexpr_conv))
          vals.emplace_back(op);
        else
          return nullptr;
        ++opi;
      }
    }
    auto val = make_unique<AggregateValue>(*ty, move(vals));
    auto ret = val.get();
    current_fn->addConstant(move(val));
    return ret;
  }

  if (auto cnst = dyn_cast<llvm::ConstantDataSequential>(v)) {
    vector<Value*> vals;
    for (unsigned i = 0, e = cnst->getNumElements(); i != e; ++i) {
      if (auto op = get_operand(cnst->getElementAsConstant(i), constexpr_conv))
        vals.emplace_back(op);
      else
        return nullptr;
    }
    auto val = make_unique<AggregateValue>(*ty, move(vals));
    auto ret = val.get();
    current_fn->addConstant(move(val));
    return ret;
  }

  if (auto cnst = dyn_cast<llvm::ConstantAggregateZero>(v)) {
    vector<Value*> vals;
    for (unsigned i = 0, e = cnst->getNumElements(); i != e; ++i) {
      if (auto op = get_operand(cnst->getElementValue(i), constexpr_conv))
        vals.emplace_back(op);
      else
        return nullptr;
    }
    auto val = make_unique<AggregateValue>(*ty, move(vals));
    auto ret = val.get();
    current_fn->addConstant(move(val));
    return ret;
  }

  if (auto cexpr = dyn_cast<llvm::ConstantExpr>(v)) {
    return constexpr_conv(cexpr);
  }

  return nullptr;
}


void add_identifier(const llvm::Value &llvm, Value &v) {
  identifiers.emplace(&llvm, &v);
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

void reset_state(Function &f) {
  current_fn = &f;
  identifiers.clear();
  value_names.clear();
  globalvars.clear();
  value_id_counter = 0;
}

}
