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

vector<unique_ptr<IntType>> int_types;
vector<unique_ptr<PtrType>> ptr_types;

// cache complex types
unordered_map<const llvm::Type*, unique_ptr<Type>> type_cache;
unsigned type_id_counter; // for unnamed types

ostream *out;

const llvm::DataLayout *DL;

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
    return &Type::halfTy;
  case llvm::Type::FloatTyID:
    return &Type::floatTy;
  case llvm::Type::DoubleTyID:
    return &Type::doubleTy;
  case llvm::Type::FP128TyID:
    return &Type::quadTy;
  case llvm::Type::BFloatTyID:
    return &Type::bfloatTy;

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
  case llvm::Type::FixedVectorTyID: {
    auto &cache = type_cache[ty];
    if (!cache) {
      auto vty = cast<llvm::VectorType>(ty);
      auto elems = vty->getElementCount().getKnownMinValue();
      auto ety = llvm_type2alive(vty->getElementType());
      if (!ety || elems > 1024)
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


static llvm::ExitOnError ExitOnErr;

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

}
