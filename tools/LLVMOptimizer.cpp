#include "LLVMOptimizer.h"

using namespace llvm;

void optimize_module(llvm::StringRef optArgs, llvm::Module *M) {
  llvm::LoopAnalysisManager LAM;
  llvm::FunctionAnalysisManager FAM;
  llvm::CGSCCAnalysisManager CGAM;
  llvm::ModuleAnalysisManager MAM;
  llvm::PassBuilder PB;

  llvm::ModulePassManager MPM;

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
  MPM.run(*M, MAM);
}

void optimize_function(llvm::StringRef optArgs, llvm::Function *func) {
  optimize_module(optArgs, func->getParent());
}