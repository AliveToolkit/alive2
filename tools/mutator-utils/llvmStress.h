#pragma once
//===- llvm-stress.cpp - Generate RandomFromLLVMStress LL files to stress-test
//LLVM -----===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This program is a utility that generates RandomFromLLVMStress .ll files to
// stress-test different components in LLVM.
//
//===----------------------------------------------------------------------===//
// This file is updated from llvm-stress.cpp
// used for generating a RandomFromLLVMStress and independent basic block

#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/APInt.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/Twine.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CallingConv.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/IRPrintingPasses.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Support/WithColor.h"
#include "llvm/Support/raw_ostream.h"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

namespace {
using namespace llvm;

/// A utility class to provide a pseudo-RandomFromLLVMStress number generator
/// which is the same across all platforms. This is somewhat close to the libc
/// implementation. Note: This is not a cryptographically secure
/// pseudoRandomFromLLVMStress number generator.
class RandomFromLLVMStress {
public:
  /// C'tor
  RandomFromLLVMStress(unsigned _seed) : Seed(_seed) {}

  /// Return a RandomFromLLVMStress integer, up to a
  /// maximum of 2**19 - 1.
  uint32_t Rand() {
    uint32_t Val = Seed + 0x000b07a1;
    Seed = (Val * 0x3c7c0ac1);
    // Only lowest 19 bits are RandomFromLLVMStress-ish.
    return Seed & 0x7ffff;
  }

  /// Return a RandomFromLLVMStress 64 bit integer.
  uint64_t Rand64() {
    uint64_t Val = Rand() & 0xffff;
    Val |= uint64_t(Rand() & 0xffff) << 16;
    Val |= uint64_t(Rand() & 0xffff) << 32;
    Val |= uint64_t(Rand() & 0xffff) << 48;
    return Val;
  }

  /// Rand operator for STL algorithms.
  ptrdiff_t operator()(ptrdiff_t y) {
    return Rand64() % y;
  }

  /// Make this like a C++11 RandomFromLLVMStress device
  using result_type = uint32_t;

  static constexpr result_type min() {
    return 0;
  }
  static constexpr result_type max() {
    return 0x7ffff;
  }

  uint32_t operator()() {
    uint32_t Val = Rand();
    assert(Val <= max() && "RandomFromLLVMStress value out of range");
    return Val;
  }

private:
  unsigned Seed;
};

/// A base class, implementing utilities needed for
/// modifying and adding new RandomFromLLVMStress instructions.
struct Modifier {
  /// Used to store the RandomFromLLVMStressly generated values.
  using PieceTable = std::vector<Value *>;

public:
  /// C'tor
  Modifier(PieceTable *PT, RandomFromLLVMStress *R, Module* module)
      : PT(PT), Ran(R), module(module) {}

  /// virtual D'tor to silence warnings.
  virtual ~Modifier() = default;

  /// Add a new instruction.
  virtual void Act() = 0;

  /// Add N new instructions,
  virtual void ActN(unsigned n) {
    for (unsigned i = 0; i < n; ++i)
      Act();
  }

  static void setInsertPoint(Instruction *inst) {
    insertPoint = inst;
  }
  static Instruction *getInsertPoint() {
    return insertPoint;
  }
  //clear both glbVars and glbFuncs;
  static void init(){
    glbVals.clear();
    glbFuncs.clear();
  }

protected:
  static Instruction *insertPoint;
  /// Return a RandomFromLLVMStress integer.
  uint32_t getRandomFromLLVMStress() {
    return Ran->Rand();
  }

  Value* getOrInsertFromGlobalFunction(Type* ty){ 
    assert(ty->isFunctionTy()&& "getOrInsertFromGlobalFunction should be a function!");
    if(glbFuncs.empty()){
      for(auto it=module->begin();it!=module->end();++it){
        glbFuncs.push_back(&*it);
      }
    }
    bool shouldInsert=getRandomFromLLVMStress()&1,found=false;
    Value* res=nullptr;
    if(!shouldInsert&&!glbFuncs.empty()){
      for(size_t i=0,pos=getRandomFromLLVMStress()%glbFuncs.size();i<glbFuncs.size();++i,++pos){
        if(pos==glbFuncs.size()){
          pos=0;
        }
        if(glbFuncs[pos]->getType()==ty){
          found=true;
          res=glbFuncs[pos];
          break;
        }
      }
    }
    if(shouldInsert||!found){
      std::string varName=std::string("aliveMutateGeneratedFunc")+std::to_string(glbFuncs.size());
      module->getOrInsertFunction(varName,ty);
      Function* glbFunc=module->getFunction(varName);
      glbFuncs.push_back(glbFunc);
      glbFunc->setLinkage(GlobalValue::ExternalLinkage);
      res=glbFunc;
    }
    return res;
  }

  Value* getOrInsertFromGlobalValue(Type* ty,bool createLoadInst=true){
    //scalable vectors cannot be global variables
    if(llvm::isa<llvm::ScalableVectorType>(ty)){
      return nullptr;      
    }
    if(ty->isFunctionTy()){
      return getOrInsertFromGlobalFunction(ty);
    }
    if(glbVals.empty()){
      for(auto it=module->global_begin();it!=module->global_end();++it){
        glbVals.push_back(&*it);
      }
    }
    bool shouldInsert=getRandomFromLLVMStress()&1,found=false;
    Value* res=nullptr;
    if(!shouldInsert&&!glbVals.empty()){
      for(size_t i=0,pos=getRandomFromLLVMStress()%glbVals.size();i<glbVals.size();++i,++pos){
        if(pos==glbVals.size()){
          pos=0;
        }
        if(glbVals[pos]->getType()==ty){
          found=true;
          res=glbVals[pos];
          break;
        }
      }
    }
    if(shouldInsert||!found){
      std::string varName=std::string("aliveMutateGeneratedGlobalVariable")+std::to_string(glbVals.size());
      assert(nullptr==module->getGlobalVariable(varName)&&"The glb var already exists!");
      res=module->getOrInsertGlobal(varName,ty);
      GlobalVariable* glbVal=module->getGlobalVariable(varName);
      glbVals.push_back(glbVal);
      glbVal->setLinkage(GlobalValue::ExternalLinkage);
      if(createLoadInst){
        Value *V = new LoadInst(ty, glbVal, "L",
                              insertPoint);
        PT->push_back(V);
        res=V;
      }
    }
    return res;
  }

  /// Return a RandomFromLLVMStress value from the list of known values.
  Value *getRandomFromLLVMStressVal() {
    assert(PT->size());
    return PT->at(getRandomFromLLVMStress() % PT->size());
  }

  Constant *getRandomFromLLVMStressConstant(Type *Tp) {
    if (Tp->isIntegerTy()) {
      if (getRandomFromLLVMStress() & 1)
        return ConstantInt::getAllOnesValue(Tp);
      return ConstantInt::getNullValue(Tp);
    } else if (Tp->isFloatingPointTy()) {
      if (getRandomFromLLVMStress() & 1)
        return ConstantFP::getAllOnesValue(Tp);
      return ConstantFP::getNullValue(Tp);
    }else if(Tp->isPointerTy()){
      assert(!Tp->isOpaquePointerTy()&&"Cannot create constant from an opaque pointer!");
      llvm::Value* result=getOrInsertFromGlobalValue(Tp->getNonOpaquePointerElementType(),false);
      assert(llvm::isa<llvm::Constant>(result)&&"should be a constant in getRandomConstant!");
      return (Constant*)result;
    }else{
      return nullptr;
    }
  }

  /// Return a RandomFromLLVMStress value with a known type.
  Value *getRandomFromLLVMStressValue(Type *Tp) {
    unsigned index = getRandomFromLLVMStress();
    for (unsigned i = 0; i < PT->size(); ++i) {
      Value *V = PT->at((index + i) % PT->size());
      if (V->getType() == Tp)
        return V;
    }

    // If the requested type was not found, generate a constant value.
    if (Tp->isIntegerTy()) {
      if (getRandomFromLLVMStress() & 1)
        return ConstantInt::getAllOnesValue(Tp);
      return ConstantInt::getNullValue(Tp);
    } else if (Tp->isFloatingPointTy()) {
      if (getRandomFromLLVMStress() & 1)
        return ConstantFP::getAllOnesValue(Tp);
      return ConstantFP::getNullValue(Tp);
    } else if (Tp->isVectorTy()) {
      if(llvm::isa<ScalableVectorType>(Tp)){
        return ConstantAggregateZero::get(Tp);
      }
      auto *VTp = cast<FixedVectorType>(Tp);

      std::vector<Constant *> TempValues;
      TempValues.reserve(VTp->getNumElements());
      for (unsigned i = 0; i < VTp->getNumElements(); ++i){
        TempValues.push_back(
            getRandomFromLLVMStressConstant(VTp->getScalarType()));
      }

      ArrayRef<Constant *> VectorValue(TempValues);
      llvm::Constant* vec=ConstantVector::get(VectorValue);
      return vec;
    }
    return getOrInsertFromGlobalValue(Tp);
  }

  /// Return a RandomFromLLVMStress value of any pointer type.
  Value *getRandomFromLLVMStressPointerValue(bool canBeFuncTy=true) {
    unsigned index = getRandomFromLLVMStress();
    for (unsigned i = 0; i < PT->size(); ++i) {
      Value *V = PT->at((index + i) % PT->size());
      if (V->getType()->isPointerTy()){
        //should not be a opaque pointer if we want to access its internal type
        if(!canBeFuncTy&&!V->getType()->isOpaquePointerTy()&&V->getType()->getNonOpaquePointerElementType()->isFunctionTy()){
          continue;
        }
        return V;
      }
    }
    llvm::Type* ty=pickPointerType();
    if(ty==nullptr||!ty->isSized()){
      return nullptr;
    }
    return getOrInsertFromGlobalValue(ty);
  }

  /// Return a RandomFromLLVMStress value of any vector type.
  Value *getRandomFromLLVMStressVectorValue() {
    unsigned index = getRandomFromLLVMStress();
    for (unsigned i = 0; i < PT->size(); ++i) {
      Value *V = PT->at((index + i) % PT->size());
      if (isa<FixedVectorType>(V->getType()))
        return V;
    }
    llvm::Type* ty=pickVectorType();
    return getOrInsertFromGlobalValue(ty);
  }

  /// Pick a RandomFromLLVMStress type.
  Type *pickType() {
    return (getRandomFromLLVMStress() & 1) ? pickVectorType()
                                           : pickScalarType();
  }

  /// Pick a RandomFromLLVMStress pointer type.
  Type *pickPointerType() {
    Type *Ty = pickType();
    return PointerType::get(Ty, 0);
  }

  /// Pick a RandomFromLLVMStress vector type.
  Type *pickVectorType(unsigned len = (unsigned)-1) {
    // Pick a RandomFromLLVMStress vector width in the range 2**0 to 2**4.
    // by adding two RandomFromLLVMStresss we are generating a normal-like
    // distribution around 2**3.
    unsigned width = 1 << ((getRandomFromLLVMStress() % 3) +
                           (getRandomFromLLVMStress() % 3));
    Type *Ty;

    // Vectors of x86mmx are illegal; keep trying till we get something else.
    do {
      Ty = pickScalarType();
    } while (Ty->isX86_MMXTy());

    if (len != (unsigned)-1)
      width = len;
    return FixedVectorType::get(Ty, width);
  }

  /// Pick a RandomFromLLVMStress scalar type.
  Type *pickScalarType() {
    static std::vector<Type *> ScalarTypes;
    if (ScalarTypes.empty()) {
      LLVMContext& Context=module->getContext();
      ScalarTypes.assign({Type::getInt1Ty(Context), Type::getInt8Ty(Context),
                          Type::getInt16Ty(Context), Type::getInt32Ty(Context),
                          Type::getInt64Ty(Context), Type::getFloatTy(Context),
                          Type::getDoubleTy(Context)});
    }

    return ScalarTypes[getRandomFromLLVMStress() % ScalarTypes.size()];
  }
  /// Value table
  PieceTable *PT;

  /// RandomFromLLVMStress number generator
  RandomFromLLVMStress *Ran;

  /// Module
  Module* module;
  static SmallVector<Value*> glbVals;
  static SmallVector<Value*> glbFuncs;
};
Instruction *Modifier::insertPoint = nullptr;
SmallVector<Value*> Modifier::glbVals;
SmallVector<Value*> Modifier::glbFuncs;


struct LoadModifier : public Modifier {
  LoadModifier(PieceTable *PT, RandomFromLLVMStress *R,  Module* module)
      : Modifier(PT, R, module) {}

  void Act() override {
    // Try to use predefined pointers. If non-exist, use undef pointer value;
    Value *Ptr = getRandomFromLLVMStressPointerValue();
    llvm::Type* ty=Ptr->getType();
    if(ty->isOpaquePointerTy()){
      return;
    }
    Value *V = new LoadInst(Ptr->getType()-> getNonOpaquePointerElementType(), Ptr, "L",
                            insertPoint);
    PT->push_back(V);
  }
};

struct StoreModifier : public Modifier {
  StoreModifier(PieceTable *PT, RandomFromLLVMStress *R,  Module* module)
      : Modifier(PT, R, module) {}

  void Act() override {
    // Try to use predefined pointers. If non-exist, use undef pointer value;
    Value *Ptr = getRandomFromLLVMStressPointerValue(false);
    //The ptr should have a size 
    if(Ptr==nullptr||Ptr->getType()->isOpaquePointerTy()||
      !Ptr->getType()->isSized()||!Ptr->getType()->getNonOpaquePointerElementType()->isSized()){
      return;
    }
    Value *Val =
        getRandomFromLLVMStressValue(Ptr->getType()->getNonOpaquePointerElementType());
    Type *ValTy = Val->getType();
    

    // Do not store vectors of i1s because they are unsupported
    // by the codegen.
    if (ValTy->isVectorTy() && ValTy->getScalarSizeInBits() == 1)
      return;

    assert(ValTy==Ptr->getType()->getNonOpaquePointerElementType() && "type should be equal");

    new StoreInst(Val, Ptr, insertPoint);
  }
};

struct BinModifier : public Modifier {
  BinModifier(PieceTable *PT, RandomFromLLVMStress *R,  Module* module)
      : Modifier(PT, R, module) {}

  void Act() override {
    Value *Val0 = getRandomFromLLVMStressVal();
    Value *Val1 = getRandomFromLLVMStressValue(Val0->getType());

    // Don't handle pointer types.
    if (!(Val0->getType()->isIntegerTy() || Val0->getType()->isFloatingPointTy()))
      return;

    // Don't handle i1 types.
    if (Val0->getType()->getScalarSizeInBits() == 1)
      return;

    bool isFloat = Val0->getType()->getScalarType()->isFloatingPointTy();
    Instruction *Term = insertPoint;
    unsigned R = getRandomFromLLVMStress() % (isFloat ? 7 : 13);
    Instruction::BinaryOps Op;

    switch (R) {
    default:
      llvm_unreachable("Invalid BinOp");
    case 0: {
      Op = (isFloat ? Instruction::FAdd : Instruction::Add);
      break;
    }
    case 1: {
      Op = (isFloat ? Instruction::FSub : Instruction::Sub);
      break;
    }
    case 2: {
      Op = (isFloat ? Instruction::FMul : Instruction::Mul);
      break;
    }
    case 3: {
      Op = (isFloat ? Instruction::FDiv : Instruction::SDiv);
      break;
    }
    case 4: {
      Op = (isFloat ? Instruction::FDiv : Instruction::UDiv);
      break;
    }
    case 5: {
      Op = (isFloat ? Instruction::FRem : Instruction::SRem);
      break;
    }
    case 6: {
      Op = (isFloat ? Instruction::FRem : Instruction::URem);
      break;
    }
    case 7: {
      Op = Instruction::Shl;
      break;
    }
    case 8: {
      Op = Instruction::LShr;
      break;
    }
    case 9: {
      Op = Instruction::AShr;
      break;
    }
    case 10: {
      Op = Instruction::And;
      break;
    }
    case 11: {
      Op = Instruction::Or;
      break;
    }
    case 12: {
      Op = Instruction::Xor;
      break;
    }
    }

    PT->push_back(BinaryOperator::Create(Op, Val0, Val1, "B", Term));
  }
};

/// Generate constant values.
struct ConstModifier : public Modifier {
  ConstModifier(PieceTable *PT, RandomFromLLVMStress *R,  Module* module)
      : Modifier(PT, R, module) {}

  void Act() override {
    Type *Ty = pickType();

    if (Ty->isVectorTy()) {
      switch (getRandomFromLLVMStress() % 2) {
      case 0:
        if (Ty->isIntOrIntVectorTy())
          return PT->push_back(ConstantVector::getAllOnesValue(Ty));
        break;
      case 1:
        if (Ty->isIntOrIntVectorTy())
          return PT->push_back(ConstantVector::getNullValue(Ty));
      }
    }

    if (Ty->isFloatingPointTy()) {
      // Generate 128 RandomFromLLVMStress bits, the size of the (currently)
      // largest floating-point types.
      uint64_t RandomFromLLVMStressBits[2];
      for (unsigned i = 0; i < 2; ++i)
        RandomFromLLVMStressBits[i] = Ran->Rand64();

      APInt RandomFromLLVMStressInt(Ty->getPrimitiveSizeInBits(),
                                    makeArrayRef(RandomFromLLVMStressBits));
      APFloat RandomFromLLVMStressFloat(Ty->getFltSemantics(),
                                        RandomFromLLVMStressInt);

      if (getRandomFromLLVMStress() & 1)
        return PT->push_back(ConstantFP::getNullValue(Ty));
      return PT->push_back(
          ConstantFP::get(Ty->getContext(), RandomFromLLVMStressFloat));
    }

    if (Ty->isIntegerTy()) {
      switch (getRandomFromLLVMStress() % 7) {
      case 0:
        return PT->push_back(ConstantInt::get(
            Ty, APInt::getAllOnes(Ty->getPrimitiveSizeInBits())));
      case 1:
        return PT->push_back(
            ConstantInt::get(Ty, APInt::getZero(Ty->getPrimitiveSizeInBits())));
      case 2:
      case 3:
      case 4:
      case 5:
      case 6:
        PT->push_back(ConstantInt::get(Ty, getRandomFromLLVMStress()));
      }
    }
  }
};

struct AllocaModifier : public Modifier {
  AllocaModifier(PieceTable *PT, RandomFromLLVMStress *R,  Module* module)
      : Modifier(PT, R, module) {}

  void Act() override {
    Type *Tp = pickType();
    const DataLayout &DL = insertPoint->getModule()->getDataLayout();
    PT->push_back(
        new AllocaInst(Tp, DL.getAllocaAddrSpace(), "A", insertPoint));
  }
};

struct ExtractElementModifier : public Modifier {
  ExtractElementModifier(PieceTable *PT, RandomFromLLVMStress *R,
                          Module* module)
      : Modifier(PT, R, module) {}

  void Act() override {
    Value *Val0 = getRandomFromLLVMStressVectorValue();
    Value *V = ExtractElementInst::Create(
        Val0,
        ConstantInt::get(
            Type::getInt32Ty(module->getContext()),
            getRandomFromLLVMStress() %
                (((FixedVectorType*)(Val0->getType()))->getNumElements())),
        "E", insertPoint);
    PT->push_back(V);
  }
};

struct ShuffModifier : public Modifier {
  ShuffModifier(PieceTable *PT, RandomFromLLVMStress *R,  Module* module)
      : Modifier(PT, R, module) {}

  void Act() override {
    Value *Val0 = getRandomFromLLVMStressVectorValue();
    Value *Val1 = getRandomFromLLVMStressValue(Val0->getType());

    unsigned Width = cast<FixedVectorType>(Val0->getType())->getNumElements();
    std::vector<Constant *> Idxs;

    Type *I32 = Type::getInt32Ty(module->getContext());
    for (unsigned i = 0; i < Width; ++i) {
      Constant *CI =
          ConstantInt::get(I32, getRandomFromLLVMStress() % (Width * 2));
      // Pick some undef values.
      //if (!(getRandomFromLLVMStress() % 5))
        //CI = UndefValue::get(I32);
      Idxs.push_back(CI);
    }

    Constant *Mask = ConstantVector::get(Idxs);

    Value *V = new ShuffleVectorInst(Val0, Val1, Mask, "Shuff", insertPoint);
    PT->push_back(V);
  }
};

struct InsertElementModifier : public Modifier {
  InsertElementModifier(PieceTable *PT, RandomFromLLVMStress *R,
                         Module* module)
      : Modifier(PT, R, module) {}

  void Act() override {
    Value *Val0 = getRandomFromLLVMStressVectorValue();
    Value *Val1 =
        getRandomFromLLVMStressValue(Val0->getType()->getScalarType());

    Value *V = InsertElementInst::Create(
        Val0, Val1,
        ConstantInt::get(
            Type::getInt32Ty(module->getContext()),
            getRandomFromLLVMStress() %
                cast<FixedVectorType>(Val0->getType())->getNumElements()),
        "I", insertPoint);
    return PT->push_back(V);
  }
};

struct CastModifier : public Modifier {
  CastModifier(PieceTable *PT, RandomFromLLVMStress *R,  Module* module)
      : Modifier(PT, R, module) {}

  void Act() override {
    Value *V = getRandomFromLLVMStressVal();
    Type *VTy = V->getType();
    Type *DestTy = pickScalarType();
    
    if(!VTy->isIntegerTy()&&!VTy->isFloatingPointTy()){
      return;
    }
    // Handle vector casts vectors.
    if (VTy->isVectorTy()) {
      auto *VecTy = cast<FixedVectorType>(VTy);
      DestTy = pickVectorType(VecTy->getNumElements());
    }

    // no need to cast.
    if (VTy == DestTy)
      return;

    // Pointers:
    if (VTy->isPointerTy()) {
      if (!DestTy->isPointerTy())
        DestTy = PointerType::get(DestTy, 0);
      return PT->push_back(new BitCastInst(V, DestTy, "PC", insertPoint));
    }

    unsigned VSize = VTy->getScalarType()->getPrimitiveSizeInBits();
    unsigned DestSize = DestTy->getScalarType()->getPrimitiveSizeInBits();

    // Generate lots of bitcasts.
    if ((getRandomFromLLVMStress() & 1) && VSize == DestSize) {
      return PT->push_back(new BitCastInst(V, DestTy, "BC", insertPoint));
    }

    // Both types are integers:
    if (VTy->isIntOrIntVectorTy() && DestTy->isIntOrIntVectorTy()) {
      if (VSize > DestSize) {
        return PT->push_back(new TruncInst(V, DestTy, "Tr", insertPoint));
      } else {
        assert(VSize < DestSize && "Different int types with the same size?");
        if (getRandomFromLLVMStress() & 1)
          return PT->push_back(new ZExtInst(V, DestTy, "ZE", insertPoint));
        return PT->push_back(new SExtInst(V, DestTy, "Se", insertPoint));
      }
    }

    // Fp to int.
    if (VTy->isFPOrFPVectorTy() && DestTy->isIntOrIntVectorTy()) {
      if (getRandomFromLLVMStress() & 1)
        return PT->push_back(new FPToSIInst(V, DestTy, "FC", insertPoint));
      return PT->push_back(new FPToUIInst(V, DestTy, "FC", insertPoint));
    }

    // Int to fp.
    if (VTy->isIntOrIntVectorTy() && DestTy->isFPOrFPVectorTy()) {
      if (getRandomFromLLVMStress() & 1)
        return PT->push_back(new SIToFPInst(V, DestTy, "FC", insertPoint));
      return PT->push_back(new UIToFPInst(V, DestTy, "FC", insertPoint));
    }

    // Both floats.
    if (VTy->isFPOrFPVectorTy() && DestTy->isFPOrFPVectorTy()) {
      if (VSize > DestSize) {
        return PT->push_back(new FPTruncInst(V, DestTy, "Tr", insertPoint));
      } else if (VSize < DestSize) {
        return PT->push_back(new FPExtInst(V, DestTy, "ZE", insertPoint));
      }
      // If VSize == DestSize, then the two types must be fp128 and ppc_fp128,
      // for which there is no defined conversion. So do nothing.
    }
  }
};

struct SelectModifier : public Modifier {
  SelectModifier(PieceTable *PT, RandomFromLLVMStress *R,  Module* module)
      : Modifier(PT, R, module) {}

  void Act() override {
    // Try a bunch of different select configuration until a valid one is found.
    Value *Val0 = getRandomFromLLVMStressVal();
    Value *Val1 = getRandomFromLLVMStressValue(Val0->getType());
    //both operands have to be first calss type according to specification.
    //Token ty aren't applied to select and phi as well
    if(!Val0->getType()->isFirstClassType()||Val0->getType()->isTokenTy()){
      return;
    }
    Type *CondTy = Type::getInt1Ty(module->getContext());
    // If the value type is a vector, and we allow vector select, then in 50%
    // of the cases generate a vector select.
    if (isa<FixedVectorType>(Val0->getType()) &&
        (getRandomFromLLVMStress() & 1)) {
      unsigned NumElem =
          cast<FixedVectorType>(Val0->getType())->getNumElements();
      CondTy = FixedVectorType::get(CondTy, NumElem);
    }

    Value *Cond = getRandomFromLLVMStressValue(CondTy);
    Value *V = SelectInst::Create(Cond, Val0, Val1, "Sl", insertPoint);
    return PT->push_back(V);
  }
};

struct CmpModifier : public Modifier {
  CmpModifier(PieceTable *PT, RandomFromLLVMStress *R,  Module* module)
      : Modifier(PT, R, module) {}

  void Act() override {
    Value *Val0 = getRandomFromLLVMStressVal();
    Value *Val1 = getRandomFromLLVMStressValue(Val0->getType());

    if (!Val0->getType()->isIntegerTy()&&!Val0->getType()->isFloatingPointTy())
      return;
    bool fp = Val0->getType()->getScalarType()->isFloatingPointTy();

    int op;
    if (fp) {
      op = getRandomFromLLVMStress() %
               (CmpInst::LAST_FCMP_PREDICATE - CmpInst::FIRST_FCMP_PREDICATE) +
           CmpInst::FIRST_FCMP_PREDICATE;
    } else {
      op = getRandomFromLLVMStress() %
               (CmpInst::LAST_ICMP_PREDICATE - CmpInst::FIRST_ICMP_PREDICATE) +
           CmpInst::FIRST_ICMP_PREDICATE;
    }

    Value *V =
        CmpInst::Create(fp ? Instruction::FCmp : Instruction::ICmp,
                        (CmpInst::Predicate)op, Val0, Val1, "Cmp", insertPoint);
    return PT->push_back(V);
  }
};
} // namespace

class RandomCodePieceGenerator {
public:
  static void setInsertPoint(Instruction *inst) {
    Modifier::setInsertPoint(inst);
  }
  static Instruction *getInsertPoint() {
    return Modifier::getInsertPoint();
  }
  static void insertCode(unsigned codeSize) {
    Instruction *insertPoint = getInsertPoint();
    assert(insertPoint != nullptr && "insertPoint should not be nullptr!");

    // Create the value table.
    Modifier::PieceTable PT;
    RandomFromLLVMStress R(0);

    // Consider arguments as legal values.
    for (auto &arg : insertPoint->getParent()->getParent()->args())
      PT.push_back(&arg);
    Module* module=insertPoint->getModule();
    // List of modifiers which add new RandomFromLLVMStress instructions.
    std::vector<std::unique_ptr<Modifier>> Modifiers;
    Modifiers.emplace_back(new LoadModifier(&PT, &R, module));
    Modifiers.emplace_back(new StoreModifier(&PT, &R, module));
    auto SM = Modifiers.back().get();
    Modifiers.emplace_back(new ExtractElementModifier(&PT, &R, module));
    Modifiers.emplace_back(new ShuffModifier(&PT, &R, module));
    Modifiers.emplace_back(new InsertElementModifier(&PT, &R, module));
    Modifiers.emplace_back(new BinModifier(&PT, &R, module));
    Modifiers.emplace_back(new CastModifier(&PT, &R, module));
    Modifiers.emplace_back(new SelectModifier(&PT, &R, module));
    Modifiers.emplace_back(new CmpModifier(&PT, &R, module));

    int tmpNum = R.Rand() % std::min(5, (int)codeSize);
    codeSize -= tmpNum;
    // Generate the RandomFromLLVMStress instructions
    AllocaModifier{&PT, &R, module}.ActN(tmpNum); // Throw in a few allocas
    if (codeSize == 0)
      return;

    tmpNum = R.Rand() % std::min(5, (int)codeSize);
    codeSize -= tmpNum;
    ConstModifier{&PT, &R, module}.ActN(tmpNum); // Throw in a few constants

    for (unsigned i = 0; codeSize != 0 && codeSize >= Modifiers.size(); ++i) {
      for (auto &Mod : Modifiers) {
        Mod->Act();
        --codeSize;
        if (codeSize == 0) {
          return;
        }
      }
    }

    SM->ActN(codeSize); // Throw in a few stores.
  }
  static void insertCodeBefore(Instruction *inst, unsigned codeSize) {
    Modifier::init();
    setInsertPoint(inst);
    insertCode(codeSize);
  }
};
