#pragma once
#include <string>
#include <memory>
#include <vector>
#include <unordered_set>
#include "simpleMutator.h"
#include "ComplexMutatorHelper.h"

/*
  This class is used for doing complex mutations on a given file.
  Current supported operation: 
    If a instruciton A use a definition of another interger type insturciton B,
      replace B with a random generated SSA. This SSA would use definitions in context.
*/

class ComplexMutator:public Mutator{
    //domInst is used for maintain instructions which dominates current instruction. 
    //this vector would be updated when moveToNextBasicBlock, moveToNextInst and restoreBackup
    friend class ShuffleHelper;
    friend class MutateInstructionHelper;
    friend class RandomMoveHelper;


    std::unordered_set<std::string> invalidFunctions;
    std::vector<llvm::Value*> domInst;

    //some functions contain 'immarg' in their arguments. Skip those function calls.
    std::unordered_set<std::string> filterSet;
    std::string currFuncName;
    std::unique_ptr<llvm::Module> tmpCopy;
    llvm::ValueToValueMapTy vMap;
    llvm::StringMap<llvm::DominatorTree> dtMap;

    llvm::Module::iterator fit,tmpFit;
    llvm::Function::iterator bit,tmpBit;
    llvm::BasicBlock::iterator iit,tmpIit;
    void moveToNextInst();
    void moveToNextBasicBlock();
    void moveToNextFuction();
    void calcDomInst();

    bool isReplaceable(llvm::Instruction* inst);
    void moveToNextReplaceableInst();
    void resetTmpModule();
    
    llvm::SmallVector<llvm::Instruction*> lazyUpdateInsts;
    llvm::SmallVector<size_t> lazyUpdateArgPos;
    llvm::SmallVector<llvm::Type*> lazyUpdateArgTys;
    llvm::SmallVector<llvm::Value*> extraFuncArgs;
    void addFunctionArguments(const llvm::SmallVector<llvm::Type*>& tys);
    llvm::Value* getRandomConstant(llvm::Type* ty);
    llvm::Value* getRandomDominatedValue(llvm::Type* ty);
    llvm::Value* getRandomValueFromExtraFuncArgs(llvm::Type* ty);
    llvm::Value* getRandomPointerValue(llvm::Type* ty);
    llvm::SmallVector<llvm::Value* (ComplexMutator::*)(llvm::Type*)> valueFuncs;

    llvm::SmallVector<std::unique_ptr<ComplexMutatorHelper>>::iterator currHelpersIt;
    llvm::SmallVector<std::unique_ptr<ComplexMutatorHelper>> helpers;
    llvm::SmallVector<size_t> whenMoveToNextInstFuncs;
    llvm::SmallVector<size_t> whenMoveToNextBasicBlockFuncs;
    llvm::SmallVector<size_t> whenMoveToNextFuncFuncs;
    llvm::Value* getRandomValue(llvm::Type* ty);
    void setOperandRandomValue(llvm::Instruction* inst,size_t pos);
    void fixAllValues();
public:
    ComplexMutator(bool debug=false):Mutator(debug),tmpCopy(nullptr),
      valueFuncs({&ComplexMutator::getRandomConstant,&ComplexMutator::getRandomDominatedValue,&ComplexMutator::getRandomValueFromExtraFuncArgs}){
    };
    ComplexMutator(std::unique_ptr<llvm::Module> pm_,const std::unordered_set<std::string>& invalidFunctions,bool debug=false):Mutator(debug),invalidFunctions(invalidFunctions),tmpCopy(nullptr),
      valueFuncs({&ComplexMutator::getRandomConstant,&ComplexMutator::getRandomDominatedValue,&ComplexMutator::getRandomValueFromExtraFuncArgs}){
      pm=std::move(pm_);
    };
    ComplexMutator(std::unique_ptr<llvm::Module> pm_,bool debug=false):Mutator(debug),tmpCopy(nullptr),
      valueFuncs({&ComplexMutator::getRandomConstant,&ComplexMutator::getRandomDominatedValue,&ComplexMutator::getRandomValueFromExtraFuncArgs}){
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

