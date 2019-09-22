// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "llvm_util/utils.h"
#include "ir/constant.h"
#include "ir/function.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include <unordered_map>
#include <utility>
#include <vector>

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

ostream *out;

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


Value* make_intconst(uint64_t val, int bits) {
  auto c = make_unique<IntConst>(get_int_type(bits), val);
  auto ret = c.get();
  current_fn->addConstant(move(c));
  return ret;
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

  if (isa<llvm::ConstantPointerNull>(v)) {
    auto val = make_unique<NullPointerValue>(*ty);
    auto ret = val.get();
    current_fn->addConstant(move(val));
    return ret;
  }

  if (auto cnst = dyn_cast<llvm::ConstantAggregate>(v)) {
    vector<Value*> vals;
    for (auto I = cnst->op_begin(), E = cnst->op_end(); I != E; ++I) {
      vals.emplace_back(get_operand(*I));
    }
    auto val = make_unique<AggregateConst>(*ty, move(vals));
    auto ret = val.get();
    current_fn->addConstant(move(val));
    return ret;
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


void init_llvm_utils(ostream &os) {
  out = &os;
  type_id_counter = 0;
  int_types.resize(65);
  int_types[1] = make_unique<IntType>("i1", 1);
  ptr_types.emplace_back(make_unique<PtrType>(0));
}

void destroy_llvm_utils() {
  int_types.clear();
  ptr_types.clear();
  type_cache.clear();
  value_names.clear();
  identifiers.clear();
}

void reset_state(Function &f) {
  current_fn = &f;
  identifiers.clear();
  value_names.clear();
  value_id_counter = 0;
}

}
