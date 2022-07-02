#pragma once
#include "mutator_helper.h"
#include "tools/mutator-utils/util.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/StringSet.h"
#include "llvm/ADT/Triple.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Dominators.h"
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

  virtual void eraseFunctionInModule(const std::string &funcName) {
    if (pm != nullptr) {
      if (llvm::Function *func = pm->getFunction(funcName); func != nullptr) {
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

/*class MutationHelper;
class ShuffleHelper;
class MutateInstructionHelper;
class RandomMoveHelper;
class RandomCodeInserterHelper;
class FunctionCallInlineHelper;
class FunctionAttributeHelper;
class VoidFunctionCallRemoveHelper;
class GEPHelper;
class BinaryInstructionHelper;*/

/*
  This class is responsible for generating different function mutants.
*/
class FunctionMutator {
  friend class ShuffleHelper;
  friend class MutateInstructionHelper;
  friend class RandomMoveHelper;
  friend class RandomCodeInserterHelper;
  friend class FunctionCallInlineHelper;
  friend class FunctionAttributeHelper;
  friend class VoidFunctionCallRemoveHelper;
  friend class GEPHelper;
  friend class BinaryInstructionHelper;

  llvm::Function *currentFunction, *functionInTmp;
  llvm::ValueToValueMapTy &vMap;
  llvm::Function::iterator bit, bitInTmp;
  llvm::BasicBlock::iterator iit, iitInTmp;

  // domInst is used for maintain instructions which dominates current
  // instruction. this vector would be updated when moveToNextBasicBlock,
  // moveToNextInst and restoreBackup

  DominatedValueVector domVals;
  llvm::SmallVector<llvm::Value *> extraValues;
  const llvm::StringSet<> &filterSet;
  const llvm::SmallVector<llvm::Value *> &globals;
  llvm::DominatorTree DT;
  std::shared_ptr<llvm::Module> tmpCopy;
  void moveToNextInstruction();
  void moveToNextBasicBlock();
  void moveToNextMutant();
  void resetIterator();
  void calcDomVals();

  llvm::SmallVector<std::unique_ptr<MutationHelper>> helpers;
  llvm::SmallVector<size_t> whenMoveToNextInstFuncs;
  llvm::SmallVector<size_t> whenMoveToNextBasicBlockFuncs;
  llvm::SmallVector<size_t> whenMoveToNextFuncFuncs;

  void initAtNewBasicBlock();
  void initAtNewInstruction();
  void initAtFunctionEntry();

  llvm::SmallVector<llvm::Instruction *> lazyUpdateInsts;
  llvm::SmallVector<size_t> lazyUpdateArgPos;
  llvm::SmallVector<llvm::Type *> lazyUpdateArgTys;

  void setOperandRandomValue(llvm::Instruction *inst, size_t pos);
  void addFunctionArguments(const llvm::SmallVector<llvm::Type *> &tys,
                            llvm::ValueToValueMapTy &VMap);
  void fixAllValues(llvm::SmallVector<llvm::Value *> &vals);

  llvm::Value *getRandomConstant(llvm::Type *ty);
  llvm::Value *getRandomDominatedValue(llvm::Type *ty);
  llvm::Value *getRandomValueFromExtraValue(llvm::Type *ty);
  llvm::Value *getRandomPointerValue(llvm::Type *ty);
  llvm::Value *getRandomFromGlobal(llvm::Type *ty);
  llvm::SmallVector<llvm::Value *(FunctionMutator::*)(llvm::Type *)> valueFuncs;
  llvm::Value *getRandomValue(llvm::Type *ty);

public:
  llvm::Function *getCurrentFunction() {
    return currentFunction;
  }
  void resetTmpCopy(std::shared_ptr<llvm::Module> copy);
  FunctionMutator(llvm::Function *currentFunction,
                  llvm::ValueToValueMapTy &vMap,
                  const llvm::StringSet<> &filterSet,
                  const llvm::SmallVector<llvm::Value *> &globals)
      : currentFunction(currentFunction), vMap(vMap), filterSet(filterSet),
        globals(globals),
        valueFuncs({&FunctionMutator::getRandomConstant,
                    &FunctionMutator::getRandomDominatedValue,
                    &FunctionMutator::getRandomValueFromExtraValue}) {
    bit = currentFunction->begin();
    iit = bit->begin();
    for (auto it = currentFunction->arg_begin();
         it != currentFunction->arg_end(); ++it) {
      domVals.push_back(&*it);
    }
    DT = llvm::DominatorTree(*currentFunction);
    calcDomVals();
    moveToNextMutant();
  }
  llvm::Function *getCurrentFunction() const {
    return currentFunction;
  }
  static bool canMutate(const llvm::Instruction &inst,
                        const llvm::StringSet<> &filterSet);
  static bool canMutate(const llvm::BasicBlock &block,
                        const llvm::StringSet<> &filterSet);
  static bool canMutate(const llvm::Function *function,
                        const llvm::StringSet<> &filterSet);
  void mutate();
  void debug();
  // should pass the pointer itself.
  void init(std::shared_ptr<FunctionMutator> self);
};

/*
  This class is responsible for doing mutations on one module; it contains lots
  of function mutant
*/

class ModuleMutator : public Mutator {
  // some functions contain 'immarg' in their arguments. Skip those function
  // calls.
  llvm::StringSet<> filterSet, invalidFunctions;
  std::shared_ptr<llvm::Module> tmpCopy;
  bool onEveryFunction;
  llvm::ValueToValueMapTy vMap;
  llvm::SmallVector<llvm::Value *> globals;

  size_t curFunction;
  llvm::StringRef curFunctionName;
  std::vector<std::shared_ptr<FunctionMutator>> functionMutants;

  void resetTmpModule();

public:
  ModuleMutator(bool debug = false) : Mutator(debug){};
  ModuleMutator(std::shared_ptr<llvm::Module> pm_,
                const llvm::StringSet<> &invalidFunctions, bool debug = false,
                bool onEveryFunction = false)
      : Mutator(debug), invalidFunctions(invalidFunctions), tmpCopy(nullptr),
        onEveryFunction(onEveryFunction), curFunction(0) {
    pm = pm_;
  };
  ModuleMutator(std::shared_ptr<llvm::Module> pm_, bool debug = false,
                bool onEveryFunction = false)
      : Mutator(debug), tmpCopy(nullptr), onEveryFunction(onEveryFunction),
        curFunction(0) {
    pm = pm_;
  }
  ~ModuleMutator(){};
  virtual bool init() override;
  virtual void mutateModule(const std::string &outputFileName) override;
  virtual std::string getCurrentFunction() const override {
    if (onEveryFunction) {
      assert("cannot get current function under onEveryFunction mode!");
    }
    return curFunctionName.str();
  }
  virtual void saveModule(const std::string &outputFileName) override;
  virtual std::shared_ptr<llvm::Module> getModule() override {
    return tmpCopy;
  }
  virtual void setModule(std::shared_ptr<llvm::Module> ptr) override {
    tmpCopy = ptr;
  }
  virtual void eraseFunctionInModule(const std::string &funcName) override {
    if (tmpCopy != nullptr) {
      if (llvm::Function *func = pm->getFunction(funcName); func != nullptr) {
        func->eraseFromParent();
      }
    }
  }
};