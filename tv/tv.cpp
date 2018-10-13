// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/function.h"
#include "smt/smt.h"
#include "smt/solver.h"
#include "tools/transform.h"
#include "util/config.h"
#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

using namespace IR;
using namespace tools;
using namespace util;
using namespace std;
using namespace llvm;

namespace {

struct TVPass : public FunctionPass {
  static char ID;

  //smt::smt_initializer smt_init; move out
  //TransformPrintOpts print_opts;
  bool FatalErrors = true;

  TVPass() : FunctionPass(ID) {}

  bool runOnFunction(llvm::Function &F) override {
    outs() << "verifying " << F.getName() << '\n';
    if (/* !V->verify(F)*/ false && FatalErrors) {
      errs() << "in function " << F.getName() << '\n'; 
      report_fatal_error("Broken function found, compilation aborted!");
    }
    return false;
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesAll();
  }
};

}

char TVPass::ID = 0;
static RegisterPass<TVPass> X("tv", "Translation Validator", false, false);
