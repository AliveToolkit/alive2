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

class ModuleIterator{
llvm::Module::iterator fit;
llvm::Function::iterator bit;
llvm::BasicBlock::iterator iit;
int functionDistance,basicBlockDistance,instructionDistance;
public:
  ModuleIterator(llvm::Module::iterator fit, 
                 llvm::Function::iterator bit,
                 llvm::BasicBlock::iterator iit):fit(fit),bit(bit),iit(iit){
                   functionDistance=basicBlockDistance=instructionDistance=0;
                 }
  ModuleIterator(llvm::Module* pm,const ModuleIterator& mit){
    fit=pm->begin();
    functionDistance=mit.functionAdvanced();
    for(int i=0;i<functionDistance;++i)++fit;
    bit=fit->begin();
    basicBlockDistance=mit.basicBlockAdvanced();
    for(int i=0;i<basicBlockDistance;++i)++bit;
    iit=bit->begin();
    instructionDistance=mit.instructionAdvanced();
    for(int i=0;i<instructionDistance;++i)++iit;
  }
  ~ModuleIterator(){};
  int functionAdvanced()const{return functionDistance;}
  int basicBlockAdvanced()const{return basicBlockDistance;}
  int instructionAdvanced()const{return instructionDistance;}
  void moveToNextInstruction();
  void moveToNextBasicBlock();
  void moveToNextFuction();
};
