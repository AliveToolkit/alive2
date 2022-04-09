#pragma once
#include "ComplexMutatorHelper.h"
#include "simpleMutator.h"
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
  This class is used for doing complex mutations on a given file.
  Current supported operation:
    If a instruciton A use a definition of another interger type insturciton B,
      replace B with a random generated SSA. This SSA would use definitions in
  context.
*/

class ComplexMutator : public Mutator {
  // domInst is used for maintain instructions which dominates current
  // instruction. this vector would be updated when moveToNextBasicBlock,
  // moveToNextInst and restoreBackup
  friend class ShuffleHelper;
  friend class MutateInstructionHelper;
  friend class RandomMoveHelper;
  friend class RandomCodeInserterHelper;
  friend class FunctionCallInlineHelper;

  std::unordered_set<std::string> invalidFunctions;
  /**
   * 1. time point of starting and deleting backup.
   * 2. update those class updates domInst
   */
  DominatedValueVector domInst;

  // some functions contain 'immarg' in their arguments. Skip those function
  // calls.
  std::unordered_set<std::string> filterSet;
  std::string currFuncName;
  std::unique_ptr<llvm::Module> tmpCopy;
  llvm::ValueToValueMapTy vMap;
  llvm::StringMap<llvm::DominatorTree> dtMap;

  llvm::Module::iterator fit, tmpFit;
  llvm::Function::iterator bit, tmpBit;
  llvm::BasicBlock::iterator iit, tmpIit;
  void moveToNextInst();
  void moveToNextBasicBlock();
  void moveToNextFuction();
  void calcDomInst();

  bool isReplaceable(llvm::Instruction *inst);
  void moveToNextReplaceableInst();
  void resetTmpModule();

  llvm::SmallVector<llvm::Instruction *> lazyUpdateInsts;
  llvm::SmallVector<size_t> lazyUpdateArgPos;
  llvm::SmallVector<llvm::Type *> lazyUpdateArgTys;
  llvm::SmallVector<llvm::Value *> extraValue;
  void addFunctionArguments(const llvm::SmallVector<llvm::Type *> &tys);
  llvm::Value *getRandomConstant(llvm::Type *ty);
  llvm::Value *getRandomDominatedValue(llvm::Type *ty);
  llvm::Value *getRandomValueFromExtraValue(llvm::Type *ty);
  llvm::Value *getRandomPointerValue(llvm::Type *ty);
  llvm::SmallVector<llvm::Value *(ComplexMutator::*)(llvm::Type *)> valueFuncs;

  llvm::SmallVector<std::unique_ptr<ComplexMutatorHelper>> helpers;
  llvm::SmallVector<int> helpersPossbility;
  llvm::SmallVector<size_t> whenMoveToNextInstFuncs;
  llvm::SmallVector<size_t> whenMoveToNextBasicBlockFuncs;
  llvm::SmallVector<size_t> whenMoveToNextFuncFuncs;
  llvm::Value *getRandomValue(llvm::Type *ty);
  void setOperandRandomValue(llvm::Instruction *inst, size_t pos);
  void fixAllValues();

public:
  ComplexMutator(bool debug = false)
      : Mutator(debug), tmpCopy(nullptr),
        valueFuncs({&ComplexMutator::getRandomConstant,
                    &ComplexMutator::getRandomDominatedValue,
                    &ComplexMutator::getRandomValueFromExtraValue}){};
  ComplexMutator(std::unique_ptr<llvm::Module> pm_,
                 const std::unordered_set<std::string> &invalidFunctions,
                 bool debug = false)
      : Mutator(debug), invalidFunctions(invalidFunctions), tmpCopy(nullptr),
        valueFuncs({&ComplexMutator::getRandomConstant,
                    &ComplexMutator::getRandomDominatedValue,
                    &ComplexMutator::getRandomValueFromExtraValue}) {
    pm = std::move(pm_);
  };
  ComplexMutator(std::unique_ptr<llvm::Module> pm_, bool debug = false)
      : Mutator(debug), tmpCopy(nullptr),
        valueFuncs({&ComplexMutator::getRandomConstant,
                    &ComplexMutator::getRandomDominatedValue,
                    &ComplexMutator::getRandomValueFromExtraValue}) {
    pm = std::move(pm_);
  }
  ~ComplexMutator(){};
  virtual bool init() override;
  virtual void mutateModule(const std::string &outputFileName) override;
  virtual std::string getCurrentFunction() const override {
    return currFuncName;
  }
  virtual void saveModule(const std::string &outputFileName) override;
  virtual std::unique_ptr<llvm::Module> getModule() override {
    return std::move(tmpCopy);
  }
  virtual void setModule(std::unique_ptr<llvm::Module> &&ptr) override {
    tmpCopy = std::move(ptr);
  }
  virtual void eraseFunctionInModule(const std::string& funcName){
    if(tmpCopy!=nullptr){
      if(llvm::Function* func=pm->getFunction(funcName);func!=nullptr){
        func->eraseFromParent();
      }
    }
  }
};
