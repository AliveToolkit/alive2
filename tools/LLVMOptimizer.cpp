#include "LLVMOptimizer.h"

using namespace llvm;

LLVMOptimizer::LLVMOptimizer(std::string optArgs) {
  this->optArgs = optArgs;
  PB.registerModuleAnalyses(MAM);
  PB.registerCGSCCAnalyses(CGAM);
  PB.registerFunctionAnalyses(FAM);
  PB.registerLoopAnalyses(LAM);
  PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

  if (optArgs == "O3") {
    MPM = PB.buildPerModuleDefaultPipeline(OptimizationLevel::O3);
  } else if (optArgs == "O2") {
    MPM = PB.buildPerModuleDefaultPipeline(OptimizationLevel::O2);
  } else if (optArgs == "O1") {
    MPM = PB.buildPerModuleDefaultPipeline(OptimizationLevel::O1);
  } else if (optArgs == "O0") {
    MPM = PB.buildPerModuleDefaultPipeline(OptimizationLevel::O0);
  } else if (optArgs == "Os") {
    MPM = PB.buildPerModuleDefaultPipeline(OptimizationLevel::Os);
  } else if (optArgs == "Oz") {
    MPM = PB.buildPerModuleDefaultPipeline(OptimizationLevel::Oz);
  } else {
    if (auto E = PB.parsePassPipeline(MPM, optArgs)) {
      llvm::errs() << E << "\n";
    }
  }
}

llvm::Module *LLVMOptimizer::optimizeModule(llvm::Module *M) {
  MPM.run(*M, MAM);
  return M;
}

llvm::Function *LLVMOptimizer::optimizeFunction(llvm::Function *func) {
  MPM.run(*(func->getParent()), MAM);
  return func;
}
