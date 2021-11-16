#pragma once
#include <string>
#include <memory>
#include <vector>
#include <unordered_set>
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
#include "tools/mutator-utils/util.h"
#include "simpleMutator.h"

/*
  This class is used for doing complex mutations on a given file.
  Current supported operation: 
    If a instruciton A use a definition of another interger type insturciton B,
      replace B with a random generated SSA. This SSA would use definitions in context.
*/

class ComplexMutator:public Mutator{
    //domInst is used for maintain instructions which dominates current instruction. 
    //this vector would be updated when moveToNextBasicBlock, moveToNextInst and restoreBackup
    std::vector<llvm::Value*> domInst;

    //some functions contain 'immarg' in their arguments. Skip those function calls.
    std::unordered_set<std::string> filterSet;
    std::string currFuncName;
    std::unique_ptr<llvm::Module> tmpCopy;
    llvm::ValueToValueMapTy vMap;
    llvm::StringMap<llvm::DominatorTree> dtMap;
    using ShuffleBlock = llvm::SmallVector<llvm::Instruction*>;
    using BasicBlockShuffleBlock = llvm::SmallVector<ShuffleBlock>;
    using FunctionShuffleBlock = llvm::SmallVector<BasicBlockShuffleBlock>;
    
    llvm::StringMap<FunctionShuffleBlock> shuffleMap;
    size_t shuffleBasicBlockIndex,shuffleBlockIndex;

    bool moved;

    llvm::Module::iterator fit,tmpFit;
    llvm::Function::iterator bit,tmpBit;
    llvm::BasicBlock::iterator iit,tmpIit;
    void moveToNextInst();
    void moveToNextBasicBlock();
    void moveToNextFuction();
    void calcDomInst();

    void shuffleBlock();
    bool isReplaceable(llvm::Instruction* inst);
    void moveToNextReplaceableInst();
    void resetTmpModule();
    void randomMoveInstruction(llvm::Instruction* inst);
    void randomMoveInstructionForward(llvm::Instruction* inst);
    void randomMoveInstructionBackward(llvm::Instruction* inst);
    void insertRandomBinaryInstruction(llvm::Instruction* inst);
    void replaceRandomUsage(llvm::Instruction* inst);

    llvm::SmallVector<llvm::Value*> addFunctionArguments(llvm::SmallVector<llvm::Type*> tys);
    llvm::Constant* getRandomConstant(llvm::Type* ty);
    llvm::Value* getRandomDominatedValue(llvm::Type* ty);
    llvm::Value* getRandomValue(llvm::Type* ty);
public:
    ComplexMutator(bool debug=false):Mutator(debug),tmpCopy(nullptr),shuffleBasicBlockIndex(0),shuffleBlockIndex(0),moved(false){};
    ComplexMutator(std::unique_ptr<llvm::Module> pm_,bool debug=false):Mutator(debug),tmpCopy(nullptr),shuffleBasicBlockIndex(0),shuffleBlockIndex(0),moved(false){
      pm=std::move(pm_);
    }
    ~ComplexMutator(){};
    virtual bool init()override;
    virtual void mutateModule(const std::string& outputFileName)override;
    virtual std::string getCurrentFunction()const override{return currFuncName;}
    virtual void saveModule(const std::string& outputFileName)override;
    virtual std::unique_ptr<llvm::Module> getModule()override{return std::move(tmpCopy);}
    virtual void setModule(std::unique_ptr<llvm::Module>&& ptr)override{tmpCopy=std::move(ptr);}
};

