#pragma once
#include "ComplexMutatorHelper.h"
#include "simpleMutator.h"
#include "llvm/ADT/StringSet.h"
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

/*
  This is a class for holding dominated value with a backup function.
         /E-F-G(backup)
  A-B-C-D(domInst)
         \E-F-G(rear)
  all operations on rear can be restored by a 'backup' operation

         /E-F-G(backup)
  A-B-C-D(domInst)
         \E-F-G(rear)

  (update on rear)

         /E-F-G(backup)
  A-B-C-D(domInst)
         \E-F-H(rear)

  (restore)

         /E-F-G(backup)
  A-B-C-D(domInst)
         \E-F-G(rear)
*/
class DominatedValueVector {
  std::vector<llvm::Value *> domInst, backup, rear;
  bool hasBackup;

public:
  DominatedValueVector() : hasBackup(false){};
  ~DominatedValueVector() {
    domInst.clear();
    backup.clear();
    rear.clear();
  }
  llvm::Value *&operator[](size_t idx) {
    if (idx < domInst.size()) {
      return domInst[idx];
    } else {
      return rear[idx - domInst.size()];
    }
  }

  void push_back_tmp(llvm::Value *val) {
    if (hasBackup) {
      rear.push_back(val);
    } else {
      domInst.push_back(val);
    }
  }

  void pop_back_tmp() {
    if (hasBackup) {
      rear.pop_back();
    } else {
      domInst.pop_back();
    }
  }

  void push_back(llvm::Value *val) {
    if (hasBackup) {
      backup.push_back(val);
      rear.push_back(val);
    } else {
      domInst.push_back(val);
    }
  }

  void pop_back() {
    if (hasBackup) {
      backup.pop_back();
      rear.pop_back();
    } else {
      domInst.pop_back();
    }
  }

  void startBackup() {
    hasBackup = true;
  }

  /**
   * rear would be clear.
   * all elements in backup would be push_back to domInst and backup clear;
   */
  void deleteBackup() {
    hasBackup = false;
    while (!backup.empty()) {
      domInst.push_back(backup.back());
      backup.pop_back();
    }
    rear.clear();
  }

  void restoreBackup() {
    if (hasBackup) {
      rear = backup;
    }
  }

  void clear() {
    hasBackup = false;
    domInst.clear();
    rear.clear();
    backup.clear();
  }

  void resize(size_t sz) {
    if (hasBackup) {
      if (sz <= domInst.size()) {
        deleteBackup();
        domInst.resize(sz);
      } else {
        rear.resize(sz - domInst.size());
        backup.resize(sz - domInst.size());
      }
    } else {
      domInst.resize(sz);
    }
  }

  llvm::Value *&back() {
    return rear.empty() ? domInst.back() : rear.back();
  }
  bool inBackup() const {
    return hasBackup;
  }
  size_t size() const {
    return domInst.size() + rear.size();
  }
  size_t tmp_size() const {
    return rear.size();
  }
  size_t empty() const {
    return domInst.empty() && (!hasBackup || rear.empty());
  }
  int find(llvm::Value *val) const {
    for (size_t i = 0; i < domInst.size(); ++i)
      if (val == domInst[i])
        return i;
    if (hasBackup) {
      for (size_t i = 0; i < rear.size(); ++i)
        if (val == rear[i])
          return i + domInst.size();
    }
    return -1;
  }
};

/*
  This class is responsible for generating different function mutants.
*/
class FunctionMutant {
friend class ShuffleHelper;
friend class MutateInstructionHelper;
friend class RandomMoveHelper;
friend class RandomCodeInserterHelper;
friend class FunctionCallInlineHelper;

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
  llvm::SmallVector<llvm::Value *(FunctionMutant::*)(llvm::Type *)> valueFuncs;
  llvm::Value *getRandomValue(llvm::Type *ty);

public:
  llvm::Function *getCurrentFunction() {
    return currentFunction;
  }
  void resetTmpCopy(std::shared_ptr<llvm::Module> copy);
  FunctionMutant(llvm::Function *currentFunction, llvm::ValueToValueMapTy &vMap,
                 const llvm::StringSet<> &filterSet,
                 const llvm::SmallVector<llvm::Value *> &globals)
      : currentFunction(currentFunction), vMap(vMap), filterSet(filterSet),
        globals(globals),
        valueFuncs({&FunctionMutant::getRandomConstant,
                    &FunctionMutant::getRandomDominatedValue,
                    &FunctionMutant::getRandomValueFromExtraValue}) {
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
  //should pass the pointer itself.
  void init(std::shared_ptr<FunctionMutant> self);
};

/*
  This class is used for doing complex mutations on a given file.
  Current supported operation:
    If a instruciton A use a definition of another interger type insturciton B,
      replace B with a random generated SSA. This SSA would use definitions in
  context.
*/

class ComplexMutator : public Mutator {
  // some functions contain 'immarg' in their arguments. Skip those function
  // calls.
  llvm::StringSet<> filterSet,invalidFunctions;
  std::shared_ptr<llvm::Module> tmpCopy;
  llvm::ValueToValueMapTy vMap;
  llvm::SmallVector<llvm::Value *> globals;

  size_t curFunction;
  std::vector<std::shared_ptr<FunctionMutant>> functionMutants;

  void resetTmpModule();

public:
  ComplexMutator(bool debug = false){};
  ComplexMutator(std::shared_ptr<llvm::Module> pm_,
                 const llvm::StringSet<> &invalidFunctions,
                 bool debug = false)
      : Mutator(debug), invalidFunctions(invalidFunctions), tmpCopy(nullptr),curFunction(0){
    pm = std::move(pm_);
  };
  ComplexMutator(std::shared_ptr<llvm::Module> pm_, bool debug = false)
      : Mutator(debug), tmpCopy(nullptr),curFunction(0) {
    pm = std::move(pm_);
  }
  ~ComplexMutator(){};
  virtual bool init() override;
  virtual void mutateModule(const std::string &outputFileName) override;
  virtual std::string getCurrentFunction() const override {
    return std::string(
        functionMutants[curFunction]->getCurrentFunction()->getName());
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
