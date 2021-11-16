#pragma once
#include <string>
#include <algorithm>
#include <vector>
#include <memory>
#include <random>
#include <ctime>
#include "llvm/IR/Module.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/Triple.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/InitializePasses.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/Error.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Verifier.h"

using std::string;

class Random{
  static std::random_device rd;
  static unsigned seed;
  static std::mt19937 mt;
  static std::uniform_int_distribution<int> dist;
  public:
    static int getRandomInt(){return dist(mt);}
    static unsigned getRandomUnsigned(){return abs(dist(mt));}
    static bool getRandomBool(){return dist(mt)&1;}
    static unsigned getSeed(){return seed;}
    static void setSeed(unsigned seed_){
      seed=seed_;
      mt=std::mt19937(seed);
    }
};

class LLVMUtil{
public:
  static void optimizeModule(llvm::Module *M);
  static void optimizeFunction(llvm::Function* f);
  static llvm::Value* insertGlobalVariable(llvm::Module* m,llvm::Type* ty);
  static void insertFunctionArguments(llvm::Function* f,llvm::SmallVector<llvm::Type*> tys,llvm::ValueToValueMapTy& VMap);
};