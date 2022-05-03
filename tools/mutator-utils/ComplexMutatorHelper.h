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

class ComplexMutator;

class ComplexMutatorHelper {
protected:
  ComplexMutator *mutator;

public:
  ComplexMutatorHelper(ComplexMutator *mutator) : mutator(mutator){};
  virtual ~ComplexMutatorHelper(){};
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

class ShuffleHelper : public ComplexMutatorHelper {
  using ShuffleBlock = llvm::SmallVector<llvm::Instruction *>;
  using BasicBlockShuffleBlock = llvm::SmallVector<ShuffleBlock>;
  using FunctionShuffleBlock = llvm::SmallVector<BasicBlockShuffleBlock>;

  llvm::StringMap<FunctionShuffleBlock> shuffleMap;
  size_t shuffleBasicBlockIndex, shuffleBlockIndex;
  void shuffleBlock();

public:
  ShuffleHelper(ComplexMutator *mutator)
      : ComplexMutatorHelper(mutator), shuffleBasicBlockIndex(0),
        shuffleBlockIndex(0){};
  virtual void init() override;
  virtual void reset() override {
    shuffleBasicBlockIndex = shuffleBlockIndex = 0;
  }
  virtual void mutate() override {
    shuffleBlock();
    ++shuffleBlockIndex;
  };
  virtual bool shouldMutate() override;
  virtual void whenMoveToNextBasicBlock() override {
    shuffleBlockIndex = 0;
  };
  virtual void whenMoveToNextFunction() override {
    shuffleBasicBlockIndex = 0;
  };
  virtual void debug() override {
    llvm::errs() << "\nInstructions shuffled\n";
  };
};

class MutateInstructionHelper : public ComplexMutatorHelper {
  bool mutated, newAdded;
  /** Try to insert new random binary instruction for int type
   *  return false if it fails to find one int parameter
   * */
  bool insertRandomBinaryInstruction(llvm::Instruction *inst);
  void replaceRandomUsage(llvm::Instruction *inst);

public:
  MutateInstructionHelper(ComplexMutator *mutator)
      : ComplexMutatorHelper(mutator), mutated(false), newAdded(false){};
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

class RandomMoveHelper : public ComplexMutatorHelper {
  bool moved;
  void randomMoveInstruction(llvm::Instruction *inst);
  void randomMoveInstructionForward(llvm::Instruction *inst);
  void randomMoveInstructionBackward(llvm::Instruction *inst);

public:
  RandomMoveHelper(ComplexMutator *mutator)
      : ComplexMutatorHelper(mutator), moved(false){};
  virtual void init() {
    moved = false;
  };
  virtual void reset() {
    moved = false;
  };
  virtual void mutate();
  virtual bool shouldMutate();
  virtual void whenMoveToNextInst() {
    moved = false;
  };
  virtual void debug() {
    llvm::errs() << "Inst was moved around";
  }
};

class RandomCodeInserterHelper : public ComplexMutatorHelper {
  bool generated;

public:
  RandomCodeInserterHelper(ComplexMutator *mutator)
      : ComplexMutatorHelper(mutator), generated(false) {}
  virtual void init() {
    generated = false;
  }
  virtual void reset() {
    generated = false;
  }
  virtual void whenMoveToNextInst(){
    generated= false;
  }
  virtual void mutate();
  virtual bool shouldMutate();
  virtual void debug() {
    llvm::errs() << "Code piece generated";
  }
};

class FunctionCallInlineHelper: public ComplexMutatorHelper{
  bool inlined;
  std::vector<std::vector<std::string>> idToFuncSet;
  llvm::StringMap<int> funcToId;
  std::string functionInlined;
public:
  FunctionCallInlineHelper(ComplexMutator* mutator):
    ComplexMutatorHelper(mutator),inlined(false) {}
  virtual void init();
  virtual void reset(){
    inlined=false;
    functionInlined.clear();
  }
  virtual void whenMoveToNextInst(){
    inlined=false;
    functionInlined.clear();
  }
  virtual void mutate();
  virtual bool shouldMutate();
  llvm::Function* getReplacedFunction();
  virtual void debug(){
    llvm::errs()<<"Function call inline with "<<functionInlined<<"\n";
  }
};