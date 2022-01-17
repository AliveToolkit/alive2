#pragma once
#include "tools/mutator-utils/util.h"
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
  std::unique_ptr<llvm::Module> pm;

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
  virtual std::unique_ptr<llvm::Module> getModule() {
    return std::move(pm);
  }
  virtual void setModule(std::unique_ptr<llvm::Module> &&ptr) {
    pm = std::move(ptr);
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

class Mutant {
public:
  Mutant() {}
  virtual ~Mutant() {}
  virtual void mutate() = 0;
  virtual void restoreMutate() = 0;
  virtual bool isBoring() const {
    return false;
  };
  virtual void print() const = 0;
};

/*
  remove void function call or invoke
*/
class VoidFunctionCallMutant : public Mutant {
  llvm::CallBase *callInst;
  llvm::Instruction *nextInst;
  bool shouldPutback;

public:
  VoidFunctionCallMutant(llvm::CallBase *callInst, llvm::Instruction *nextInst)
      : callInst(callInst), nextInst(nextInst), shouldPutback(false) {}
  virtual ~VoidFunctionCallMutant() {
    if (shouldPutback) {
      callInst->insertBefore(nextInst);
    }
  };
  virtual void mutate() override {
    callInst->removeFromParent();
    shouldPutback = true;
  }
  virtual void restoreMutate() override {
    callInst->insertBefore(nextInst);
    shouldPutback = false;
  }
  virtual bool isBoring() const {
    return true;
  }
  virtual void print() const {
    llvm::errs() << "Removed function\n";
    callInst->print(llvm::errs());
    llvm::errs() << "\nBaisc block\n";
    nextInst->getParent()->print(llvm::errs());
    llvm::errs() << "\n";
  }
};

class BinaryInstructionMutant : public Mutant {
  llvm::BinaryOperator *binaryInst, *mutatedInst;
  llvm::Instruction::BinaryOps op;
  int index;
  llvm::Value *val1, *val2;
  const static std::vector<std::function<void(llvm::BinaryOperator *)>>
      flagFunctions;
  static void doNothing(llvm::BinaryOperator *){};
  static void resetFastMathFlags(llvm::BinaryOperator *inst);
  static void resetNUWNSWFlags(llvm::BinaryOperator *inst);
  static void resetExactFlag(llvm::BinaryOperator *inst);
  const static std::unordered_map<llvm::Instruction::BinaryOps, int>
      operToIndex;
  const static std::vector<std::vector<llvm::Instruction::BinaryOps>>
      indexToOperSet;
  void replaceConstant(llvm::Value *&v) {
    llvm::Type *ty = v->getType();
    if (llvm::isa<llvm::ConstantInt>(v) && ty->isIntegerTy()) {
      v = llvm::ConstantInt::get(
          ty, Random::getRandomLLVMInt((llvm::IntegerType *)ty));
    } else if (llvm::isa<llvm::ConstantFP>(v) && ty->isFloatingPointTy()) {
      v = llvm::ConstantFP::get(ty, Random::getRandomLLVMDouble());
    }
  }
  void resetMathFlags() {
    if (mutatedInst != nullptr && index >= 0 &&
        index < (int)flagFunctions.size()) {
      flagFunctions[index](mutatedInst);
    }
  }
  void swapOperands() {
    llvm::Value *tmp = val1;
    val1 = val2;
    val2 = tmp;
  }
  void replaceOperator() {
    if (index >= 0 && index < (int)indexToOperSet.size()) {
      const std::vector<llvm::Instruction::BinaryOps> &v =
          indexToOperSet[index];
      op = v[Random::getRandomUnsigned() % v.size()];
    }
  }

public:
  BinaryInstructionMutant(llvm::BinaryOperator *binaryInst)
      : binaryInst(binaryInst), mutatedInst(nullptr) {
    op = binaryInst->getOpcode();
    if (auto it = operToIndex.find(op); it != operToIndex.end()) {
      index = it->second;
    }
    val1 = binaryInst->getOperand(0), val2 = binaryInst->getOperand(1);
  };
  virtual ~BinaryInstructionMutant() {
    if (mutatedInst != nullptr)
      binaryInst->insertBefore(mutatedInst);
  };

  virtual void mutate() {

    replaceConstant(val1);
    replaceConstant(val2);
    if (Random::getRandomBool()) {
      swapOperands();
    }
    replaceOperator();
    mutatedInst = llvm::BinaryOperator::Create(op, val1, val2, "", binaryInst);
    resetMathFlags();
    binaryInst->replaceAllUsesWith(mutatedInst);
    binaryInst->removeFromParent();
  };
  virtual void restoreMutate() {
    if (mutatedInst != nullptr) {
      binaryInst->insertBefore(mutatedInst);
      mutatedInst->replaceAllUsesWith(binaryInst);
      mutatedInst->eraseFromParent();
      mutatedInst = nullptr;
    }
  }
  virtual void print() const {
    llvm::errs() << "Orginal binary inst:\n";
    binaryInst->print(llvm::errs());
    llvm::errs() << "\nMutated binary inst:\n";
    mutatedInst->print(llvm::outs());
    llvm::errs() << "\n";
    mutatedInst->getParent()->print(llvm::errs());
  }
};

class GEPInstructionMutant : public Mutant {
  llvm::GetElementPtrInst *GEPInst;
  bool isInBounds;

public:
  GEPInstructionMutant(llvm::GetElementPtrInst *GEPInst) : GEPInst(GEPInst) {
    isInBounds = GEPInst->isInBounds();
  };
  virtual ~GEPInstructionMutant() {}

  virtual void mutate() {
    GEPInst->setIsInBounds(!isInBounds);
  };
  virtual void restoreMutate() {
    GEPInst->setIsInBounds(isInBounds);
  };
  virtual bool isBoring() const {
    return true;
  };
  virtual void print() const {
    llvm::errs() << "Original GEP inst:\n";
    GEPInst->print(llvm::outs());
    llvm::errs() << "inbounds flag reversed.\n";
  }
};

#define setFuncAttr(flag, attrName)                                            \
  if (func->hasFnAttribute(attrName)) {                                        \
    func->removeFnAttr(attrName);                                              \
  }                                                                            \
  if (flag) {                                                                  \
    func->addFnAttr(attrName);                                                 \
  }
#define setFuncParamAttr(flag, index, attrName)                                \
  if (func->hasParamAttribute(index, attrName)) {                              \
    func->removeParamAttr(index, attrName);                                    \
  }                                                                            \
  if (flag) {                                                                  \
    func->addParamAttr(index, attrName);                                       \
  }

class FunctionDefinitionMutant : public Mutant {
  llvm::Function *func;
  bool nofree;
  std::vector<int> dereferenceable;
  std::vector<bool> nocapture;

public:
  FunctionDefinitionMutant(llvm::Function *func) : func(func) {
    nofree = func->hasFnAttribute(llvm::Attribute::AttrKind::NoFree);
    for (size_t i = 0; i < func->arg_size(); ++i) {
      nocapture.push_back(
          func->hasParamAttribute(i, llvm::Attribute::AttrKind::NoCapture));
      if (func->hasParamAttribute(i,
                                  llvm::Attribute::AttrKind::Dereferenceable)) {
        dereferenceable.push_back(func->getParamDereferenceableBytes(i));
      } else {
        dereferenceable.push_back(-1);
      }
    }
  };
  virtual ~FunctionDefinitionMutant(){};

  virtual void mutate() {
    setFuncAttr(Random::getRandomBool(), llvm::Attribute::AttrKind::NoFree);
    for (size_t i = 0; i < func->arg_size(); ++i) {
      if (llvm::Argument *arg = func->getArg(i);
          arg->getType()->isPointerTy()) {
        setFuncParamAttr(Random::getRandomBool(), i,
                         llvm::Attribute::AttrKind::NoCapture);
        if (Random::getRandomBool()) {
          func->removeParamAttr(i, llvm::Attribute::AttrKind::Dereferenceable);
          func->addDereferenceableParamAttr(
              i, 1 << (Random::getRandomUnsigned() % 4));
        }
      }
    }
  }
  virtual void restoreMutate() {
    setFuncAttr(nofree, llvm::Attribute::AttrKind::NoFree);
    for (size_t i = 0; i < nocapture.size(); ++i) {
      if (llvm::Argument *arg = func->getArg(i);
          arg->getType()->isPointerTy()) {
        setFuncParamAttr(nocapture[i], i, llvm::Attribute::AttrKind::NoCapture);
        func->removeParamAttr(i, llvm::Attribute::AttrKind::Dereferenceable);
        if (dereferenceable[i] >= 0) {
          func->addDereferenceableParamAttr(i, dereferenceable[i]);
        }
      }
    }
  }
  virtual bool isBoring() const {
    return std::none_of(
        func->arg_begin(), func->arg_end(),
        [](const llvm::Argument &arg) { return arg.getType()->isPointerTy(); });
  }
  virtual void print() const {
    llvm::errs() << "Function name:\n";
    llvm::errs() << func->getName() << "\n";
    llvm::errs() << "attributes are set randomly.\n";
  }
};
#undef setFuncAttr
#undef setFuncParamAttr

/*
 * This class is used for doing simple mutations on a given file.
 Current supported opeartions on binary instructions:
    + swap operands
    + replacing operator
    + reset math flag
    + replace constant
 On GEP instructions:
    + reset inbounds flag
 On function definitions:
    + reset nofree flag
 On function parameters:
    + reset dereferenceable flag with random value (garantee is power and 2 and
 value range is [1,8])
    + reset nocapture flag
*/
class SimpleMutator : public Mutator {
  std::list<std::pair<std::unique_ptr<Mutant>, llvm::StringRef>> mutants;
  std::unordered_set<std::string> invalidFunctions;
  decltype(mutants.begin()) it;
  bool isFirstRun;

public:
  SimpleMutator(bool debug = false) : Mutator(debug), isFirstRun(true){};
  SimpleMutator(const std::unordered_set<std::string> &invalidFunctions,
                bool debug = false)
      : Mutator(debug), invalidFunctions(invalidFunctions), isFirstRun(true){};
  virtual ~SimpleMutator() {
    mutants.clear();
  };
  virtual bool init() override;
  virtual void mutateModule(const string &outputFileName) override;
  virtual std::string getCurrentFunction() const override {
    return it->second.str();
  };
  virtual void saveModule(const std::string &outputFileName) override;
};
