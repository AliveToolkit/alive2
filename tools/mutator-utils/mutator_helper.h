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
#include "llvm/IR/ValueSymbolTable.h"
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

class FunctionMutator;

class MutationHelper {
protected:
  std::shared_ptr<FunctionMutator> mutator;

public:
  MutationHelper(std::shared_ptr<FunctionMutator> mutator) : mutator(mutator){};
  virtual ~MutationHelper(){};
  virtual void init() = 0;
  virtual void reset() = 0;
  virtual void mutate() = 0;
  virtual bool shouldMutate() = 0;
  virtual void whenMoveToNextInst(){};
  virtual void whenMoveToNextBasicBlock(){};
  virtual void whenMoveToNextFunction(){};
  virtual void debug() {
    llvm::errs() << "Default debug, extended helpers should provide more "
                    "exhaustive information\n";
  }
};

class ShuffleHelper : public MutationHelper {
  using ShuffleUnit = llvm::SmallVector<llvm::Instruction *>;
  using ShuffleUnitInBasicBlock = llvm::SmallVector<ShuffleUnit>;
  using ShuffleBlockInFunction = llvm::SmallVector<ShuffleUnitInBasicBlock>;

  ShuffleBlockInFunction shuffleBlockInFunction;
  size_t shuffleUnitInBasicBlockIndex, shuffleUnitIndex;
  void shuffleCurrentBlock();

public:
  ShuffleHelper(std::shared_ptr<FunctionMutator> mutator)
      : MutationHelper(mutator), shuffleUnitInBasicBlockIndex(0),
        shuffleUnitIndex(0){};
  // we know this value after calculation in init, so return ture for now for
  // every function
  static bool canMutate(llvm::Function *func) {
    return true;
  };
  virtual void init() override;
  virtual void reset() override {
    shuffleUnitInBasicBlockIndex = shuffleUnitIndex = 0;
  }
  virtual void mutate() override {
    shuffleCurrentBlock();
    ++shuffleUnitIndex;
  };
  virtual bool shouldMutate() override;
  virtual void whenMoveToNextBasicBlock() override {
    shuffleUnitIndex = 0;
    ++shuffleUnitInBasicBlockIndex;
  };
  virtual void whenMoveToNextFunction() override {
    shuffleUnitInBasicBlockIndex = 0;
  };
  virtual void debug() override;
};

class MutateInstructionHelper : public MutationHelper {
  bool mutated, newAdded;
  /** Try to insert new random binary instruction for int type
   *  return false if it fails to find one int parameter
   * */
  bool insertRandomBinaryInstruction(llvm::Instruction *inst);
  void replaceRandomUsage(llvm::Instruction *inst);
  static bool isBasicBlockOrFunction(llvm::Value *val) {
    return llvm::isa<llvm::BasicBlock>(val) || llvm::isa<llvm::Function>(val);
  }
  static bool canMutate(llvm::Instruction *inst);

public:
  MutateInstructionHelper(std::shared_ptr<FunctionMutator> mutator)
      : MutationHelper(mutator), mutated(false), newAdded(false){};
  static bool canMutate(llvm::Function *func);
  virtual void init() override {
    mutated = newAdded = false;
  };
  virtual void reset() override {
    mutated = newAdded = false;
  };
  virtual void mutate() override;
  virtual bool shouldMutate() override;

  virtual void whenMoveToNextInst() override {
    mutated = newAdded = false;
  }
  virtual void debug() override {
    if (!newAdded) {
      llvm::errs() << "\nReplaced with a existant usage\n";
    } else {
      llvm::errs() << "\nNew Inst added\n";
    }
  }
};

class RandomMoveHelper : public MutationHelper {
  bool moved;
  void randomMoveInstruction(llvm::Instruction *inst);
  void randomMoveInstructionForward(llvm::Instruction *inst);
  void randomMoveInstructionBackward(llvm::Instruction *inst);

public:
  RandomMoveHelper(std::shared_ptr<FunctionMutator> mutator)
      : MutationHelper(mutator), moved(false){};
  virtual void init() override {
    moved = false;
  };
  virtual void reset() override {
    moved = false;
  };
  static bool canMutate(llvm::Function *func);
  virtual void mutate();
  virtual bool shouldMutate();
  virtual void whenMoveToNextInst() {
    moved = false;
  };
  virtual void debug() override;
};

class RandomCodeInserterHelper : public MutationHelper {
  bool generated;

public:
  RandomCodeInserterHelper(std::shared_ptr<FunctionMutator> mutator)
      : MutationHelper(mutator), generated(false) {}
  virtual void init() {
    generated = false;
  }
  virtual void reset() {
    generated = false;
  }
  virtual void whenMoveToNextInst() {
    generated = false;
  }
  static bool canMutate(llvm::Function *func) {
    return true;
  };
  virtual void mutate();
  virtual bool shouldMutate();
  virtual void debug() {
    llvm::errs() << "Code piece generated";
  }
};

class FunctionCallInlineHelper : public MutationHelper {
  bool inlined;
  std::vector<std::vector<std::string>> idToFuncSet;
  llvm::StringMap<int> funcToId;
  std::string functionInlined;
  static bool canMutate(llvm::Instruction *inst) {
    if (llvm::isa<llvm::CallInst>(inst)) {
      llvm::CallInst *callInst = (llvm::CallInst *)inst;
      llvm::Function *func = callInst->getCalledFunction();
      return func != nullptr && !func->isDeclaration();
    }
    return false;
  }

public:
  FunctionCallInlineHelper(std::shared_ptr<FunctionMutator> mutator)
      : MutationHelper(mutator), inlined(false) {}
  virtual void init();
  virtual void reset() {
    inlined = false;
    functionInlined.clear();
  }
  virtual void whenMoveToNextInst() {
    inlined = false;
    functionInlined.clear();
  }
  static bool canMutate(llvm::Function *func);
  virtual void mutate();
  virtual bool shouldMutate();
  llvm::Function *getReplacedFunction();
  virtual void debug() {
    llvm::errs() << "Function call inline with " << functionInlined << "\n";
  }
};

class VoidFunctionCallRemoveHelper : public MutationHelper {
  bool removed;
  std::string funcName;
  static bool canMutate(llvm::Instruction *inst) {
    if (llvm::isa<llvm::CallBase>(inst)) {
      llvm::CallBase *callInst = (llvm::CallBase *)inst;
      return callInst->getType()->isVoidTy();
    }
    return false;
  }

public:
  VoidFunctionCallRemoveHelper(std::shared_ptr<FunctionMutator> mutator)
      : MutationHelper(mutator), removed(false){};
  virtual void init() override {}
  virtual void reset() override {
    removed = false;
    funcName.clear();
  }
  virtual void mutate() override;
  virtual void whenMoveToNextInst() {
    removed = false;
    funcName.clear();
  }
  static bool canMutate(llvm::Function *func);
  virtual bool shouldMutate() override;
  virtual void debug() override;
};

class FunctionAttributeHelper : public MutationHelper {
  bool updated;
  llvm::SmallVector<size_t> ptrPos;

public:
  FunctionAttributeHelper(std::shared_ptr<FunctionMutator> mutator)
      : MutationHelper(mutator), updated(false){};
  virtual void init() override;
  virtual void reset() override {
    updated = false;
  }
  virtual void mutate() override;
  virtual bool shouldMutate() override {
    return !updated;
  }
  virtual void debug() override;
  static bool canMutate(llvm::Function *func) {
    return true;
  };
  virtual void whenMoveToNextFunction() override {
    updated = false;
  }
};

class GEPHelper : public MutationHelper {
  bool updated;

public:
  GEPHelper(std::shared_ptr<FunctionMutator> mutator)
      : MutationHelper(mutator), updated(false){};
  virtual void init() override {}
  virtual void whenMoveToNextInst() {
    updated = false;
  }
  virtual void reset() override {
    updated = false;
  }
  virtual void mutate() override;
  virtual bool shouldMutate() override;
  virtual void debug() override;
  static bool canMutate(llvm::Function *func);
};

class BinaryInstructionHelper : public MutationHelper {
  bool updated;
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
  static llvm::Instruction::BinaryOps getNewOperator(int opIndex) {
    assert(opIndex >= 0 && opIndex < (int)indexToOperSet.size() &&
           "op index should in range when get a new operator");
    const std::vector<llvm::Instruction::BinaryOps> &v =
        indexToOperSet[opIndex];
    return v[Random::getRandomUnsigned() % v.size()];
  }

  static void swapOperands(llvm::BinaryOperator *inst) {
    assert(llvm::isa<llvm::BinaryOperator>(*inst) &&
           "inst should be binary inst when swap operands");
    llvm::Value *val1 = inst->getOperand(0), *val2 = inst->getOperand(1);
    inst->setOperand(0, val2);
    inst->setOperand(1, val1);
  }

  static void resetMathFlags(llvm::BinaryOperator *inst, int opIndex) {
    assert(opIndex >= 0 && opIndex < (int)flagFunctions.size() &&
           "op index should be in range");
    flagFunctions[opIndex](inst);
  }

  static int getOpIndex(llvm::BinaryOperator *inst) {
    llvm::Instruction::BinaryOps op = inst->getOpcode();
    auto it = operToIndex.find(op);
    assert(it != operToIndex.end() && "invalid op code");
    return it->second;
  }

public:
  BinaryInstructionHelper(std::shared_ptr<FunctionMutator> mutator)
      : MutationHelper(mutator), updated(false){};
  virtual void init() override{};
  virtual void reset() override {
    updated = false;
  }
  static bool canMutate(llvm::Function *func);
  virtual void mutate() override;
  virtual bool shouldMutate() override;
  virtual void debug() override;
  virtual void whenMoveToNextInst() override {
    updated = false;
  }
};