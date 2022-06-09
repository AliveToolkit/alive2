// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "llvm_optimizer.h"
#include "llvm/Passes/PassBuilder.h"

using namespace llvm;
using namespace std;

namespace llvm_util {

string optimize_module(llvm::Module *M, string_view optArgs) {
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
    if (auto Err = PB.parsePassPipeline(MPM, optArgs))
      return toString(std::move(Err));
  }
  MPM.run(*M, MAM);
  return {};
}

}
