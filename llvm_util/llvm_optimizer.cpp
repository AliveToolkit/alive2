// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "llvm_optimizer.h"
#include "llvm/CodeGen/CommandFlags.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"

using namespace llvm;
using namespace std;

static void init_llvm_targets() {
  static bool initialized = false;
  static codegen::RegisterCodeGenFlags CFG;
  if (initialized)
    return;

  initialized = true;
  InitializeAllTargets();
  InitializeAllTargetMCs();
}

namespace llvm_util {

string optimize_module(llvm::Module &M, string_view optArgs) {
  init_llvm_targets();

  Triple ModuleTriple(M.getTargetTriple());
  unique_ptr<llvm::TargetMachine> TM;
  if (ModuleTriple.getArch()) {
    auto ETM = codegen::createTargetMachineForTriple(ModuleTriple.str());
    if (auto E = ETM.takeError())
      return toString(std::move(E));
    TM = std::move(*ETM);
  }

  LoopAnalysisManager LAM;
  FunctionAnalysisManager FAM;
  CGSCCAnalysisManager CGAM;
  ModuleAnalysisManager MAM;
  PassBuilder PB(TM.get());
  ModulePassManager MPM;

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
  MPM.run(M, MAM);
  return {};
}

}
