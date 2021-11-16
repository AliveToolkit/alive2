#include "util.h"

std::random_device Random::rd;
std::uniform_int_distribution<int> Random::dist(0,2147483647u);
unsigned Random::seed(rd());
std::mt19937 Random::mt(Random::seed);


void LLVMUtil::optimizeModule(llvm::Module *M) {
  llvm::LoopAnalysisManager LAM;
  llvm::FunctionAnalysisManager FAM;
  llvm::CGSCCAnalysisManager CGAM;
  llvm::ModuleAnalysisManager MAM;

  llvm::PassBuilder PB;
  PB.registerModuleAnalyses(MAM);
  PB.registerCGSCCAnalyses(CGAM);
  PB.registerFunctionAnalyses(FAM);
  PB.registerLoopAnalyses(LAM);
  PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

  llvm::FunctionPassManager FPM = PB.buildFunctionSimplificationPipeline(
      llvm::OptimizationLevel::O2, llvm::ThinOrFullLTOPhase::None);
  llvm::ModulePassManager MPM;
  MPM.addPass(createModuleToFunctionPassAdaptor(std::move(FPM)));
  MPM.run(*M, MAM);
}

void LLVMUtil::optimizeFunction(llvm::Function* f){
  llvm::LoopAnalysisManager LAM;
  llvm::FunctionAnalysisManager FAM;
  llvm::CGSCCAnalysisManager CGAM;
  llvm::ModuleAnalysisManager MAM;

  llvm::PassBuilder PB;
  PB.registerModuleAnalyses(MAM);
  PB.registerCGSCCAnalyses(CGAM);
  PB.registerFunctionAnalyses(FAM);
  PB.registerLoopAnalyses(LAM);
  PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

  llvm::FunctionPassManager FPM = PB.buildFunctionSimplificationPipeline(
      llvm::OptimizationLevel::O2, llvm::ThinOrFullLTOPhase::None);
  FPM.run(*f,FAM);
}


llvm::Value* LLVMUtil::insertGlobalVariable(llvm::Module* m,llvm::Type* ty){
    static const std::string GLOBAL_VAR_NAME_PREFIX="aliveMutateGlobalVar";
    static int varCount=0;
    m->getOrInsertGlobal(GLOBAL_VAR_NAME_PREFIX+std::to_string(varCount),ty);
    llvm::GlobalVariable* val=m->getGlobalVariable(GLOBAL_VAR_NAME_PREFIX+std::to_string(varCount));
    ++varCount;
    val->setLinkage(llvm::GlobalValue::LinkageTypes::ExternalLinkage);
    val->setAlignment(llvm::MaybeAlign(1));
    return val;
}

void LLVMUtil::insertFunctionArguments(llvm::Function* F,llvm::SmallVector<llvm::Type*> tys,llvm::ValueToValueMapTy& VMap){
    //uptated from llvm CloneFunction
    using llvm::Type;
    using llvm::Function;
    using llvm::FunctionType;
    using llvm::Argument;
    std::vector<Type *> ArgTypes;

    // The user might be deleting arguments to the function by specifying them in
    // the VMap.  If so, we need to not add the arguments to the arg ty vector
    //
    for (const Argument &I : F->args())
        if (VMap.count(&I) == 0) // Haven't mapped the argument to anything yet?
        ArgTypes.push_back(I.getType());
    for(const auto& ty:tys)
        ArgTypes.push_back(ty);

    // Create a new function type...
    FunctionType *FTy =
        FunctionType::get(F->getFunctionType()->getReturnType(), ArgTypes,
                            F->getFunctionType()->isVarArg());

    // Create the new function...
    Function *NewF = Function::Create(FTy, F->getLinkage(), F->getAddressSpace(),
                                        F->getName(), F->getParent());

    // Loop over the arguments, copying the names of the mapped arguments over...
    Function::arg_iterator DestI = NewF->arg_begin();
    for (const Argument &I : F->args())
        if (VMap.count(&I) == 0) {     // Is this argument preserved?
        DestI->setName(I.getName()); // Copy the name over...
        VMap[&I] = &*DestI++;        // Add mapping to VMap
        }

    llvm::SmallVector<llvm::ReturnInst *, 8> Returns; // Ignore returns cloned.
    CloneFunctionInto(NewF, F, VMap, llvm::CloneFunctionChangeType::LocalChangesOnly,
                        Returns, "", nullptr);
    std::string oldFuncName=F->getName().str(),newFuncName=NewF->getName().str();
    NewF->setName("tmpFunctionNameQuinella");
    F->setName(newFuncName);
    NewF->setName(oldFuncName);
}