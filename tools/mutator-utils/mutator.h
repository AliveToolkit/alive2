#pragma once
#include "tools/mutator-utils/util.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/Triple.h"
#include "llvm/ADT/StringSet.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/InitializePasses.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include <functional>
#include <list>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

class Mutator {
protected:
  bool debug;

  llvm::LLVMContext context;
  llvm::ExitOnError ExitOnErr;
  std::shared_ptr<llvm::Module> pm;

public:
  Mutator(bool debug = false) : debug(debug), pm(nullptr){};
  virtual ~Mutator(){};

  bool openInputFile(const string &inputFile);
  virtual bool init() = 0;
  virtual void mutateModule(const std::string &outputFileName) = 0;
  virtual void saveModule(const std::string &outputFileName) = 0;
  virtual std::string getCurrentFunction() const = 0;
  void setDebug(bool debug) {
    this->debug = debug;
  }
  virtual std::shared_ptr<llvm::Module> getModule() {
    return pm;
  }
  virtual void setModule(std::shared_ptr<llvm::Module> ptr) {
    pm = ptr;
  }

  virtual void eraseFunctionInModule(const std::string& funcName){
    if(pm!=nullptr){
      if(llvm::Function* func=pm->getFunction(funcName);func!=nullptr){
        func->eraseFromParent();
      }
    }
  }
};

class StubMutator : public Mutator {
  decltype(pm->begin()) fit;
  decltype(fit->begin()) bit;
  decltype(bit->begin()) iit;

  std::string currFunction;
  void moveToNextInst();
  void moveToNextBlock();
  void moveToNextFunction();

public:
  StubMutator(bool debug) : Mutator(debug){};
  virtual ~StubMutator(){};
  virtual bool init() override;
  virtual void mutateModule(const std::string &outputFileName) override;
  virtual void saveModule(const std::string &outputFileName) override;
  virtual std::string getCurrentFunction() const override;
};