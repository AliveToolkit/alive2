#pragma once
#include "llvmStress.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/Triple.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/InitializePasses.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Scalar/NewGVN.h"
#include "llvm/Transforms/Scalar/LICM.h"
#include "llvm/Analysis/MemorySSA.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/FunctionComparator.h"
 
#include <algorithm>
#include <climits>
#include <ctime>
#include <memory>
#include <random>
#include <string>
#include <vector>

using std::string;

class Random {
  static std::random_device rd;
  static unsigned seed;
  static std::mt19937 mt;
  static std::uniform_int_distribution<int> dist;
  static llvm::SmallVector<unsigned> usedInts;
  static llvm::SmallVector<double> usedDoubles;
  static llvm::SmallVector<float> usedFloats;
  static unsigned getExtremeInt(llvm::IntegerType *ty);
  static unsigned getBitmask(llvm::IntegerType *ty);
  static double getExtremeDouble();
  static float getExtremeFloat();
  static unsigned getUsedInt(llvm::IntegerType *ty);
  static double getUsedDouble();
  static float getUsedFloat();

public:
  static int getRandomInt() {
    return dist(mt);
  }
  static void addUsedInt(unsigned i) {
    usedInts.push_back(i);
  }
  static double getRandomDouble();
  static float getRandomFloat();
  static unsigned getRandomUnsigned() {
    return abs(dist(mt));
  }
  static unsigned getRandomUnsigned(unsigned bits) {
    return abs(dist(mt)) % (1u << bits);
  }
  static bool getRandomBool() {
    return dist(mt) & 1;
  }
  static unsigned getSeed() {
    return seed;
  }
  static void setSeed(unsigned seed_) {
    seed = seed_;
    mt = std::mt19937(seed);
  }
  static unsigned getRandomLLVMInt(llvm::IntegerType *ty);
  static double getRandomLLVMDouble();
  static float getRandomLLVMFloat();
};

class FunctionComparatorWrapper:public llvm::FunctionComparator{
public:
  FunctionComparatorWrapper(const llvm::Function* func1, const llvm::Function* func2,  llvm::GlobalNumberState *GN):
    llvm::FunctionComparator(func1, func2, GN){};
  bool isSameSignature()const{
    return  compareSignature()==0;
  }
};

class LLVMFunctionComparator{
  llvm::GlobalNumberState gn;
  FunctionComparatorWrapper wrapper;
public:
  LLVMFunctionComparator():gn(llvm::GlobalNumberState()),wrapper(FunctionComparatorWrapper(nullptr,nullptr,nullptr)){}
  bool compareSignature(const llvm::Function* func1, const llvm::Function* func2){
    gn.clear();
    wrapper=FunctionComparatorWrapper(func1,func2,&gn);
    return wrapper.isSameSignature();
  }
};

class LLVMUtil {
  static LLVMFunctionComparator comparator;
public:
  static void optimizeModule(llvm::Module *M, bool newGVN = false, bool licm=false);
  static void optimizeFunction(llvm::Function *f, bool newGVN = false,bool licm=false);
  static void removeTBAAMetadata(llvm::Module *M);
  static llvm::Value *insertGlobalVariable(llvm::Module *m, llvm::Type *ty);
  static void insertFunctionArguments(llvm::Function *f,
                                      llvm::SmallVector<llvm::Type *> tys,
                                      llvm::ValueToValueMapTy &VMap);
  static void insertRandomCodeBefore(llvm::Instruction *inst);
  static bool compareSignature(const llvm::Function* func1, const llvm::Function* func2){
    return comparator.compareSignature(func1, func2);
  }
};