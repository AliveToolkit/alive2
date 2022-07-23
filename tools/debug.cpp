// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

/*

1. Special constants
  min/max ± ∆
  Small numbers, required for peephole optimizations
  Based on context
  Bit blocks + end bits
  Reuse existing constants
2. Binary op replacement not only within a subset
3. Typecast operations
4. Create probabilities to control things that happen
  Swarm testing
5. Intrinsics / function calls
6. Insert arguments to function, reuse them
7. Swap/replace operands of different instructions
8. support more attributes
9. Randomly move instructions
10. remove void call or invoke
*/

#include "assert.h"
#include "llvm_util/llvm2alive.h"
#include "llvm_util/llvm_optimizer.h"
#include "smt/smt.h"
#include "tools/mutator-utils/mutator.h"
#include "tools/transform.h"
#include "util/version.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/Triple.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
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

#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_set>
#include <utility>

using namespace tools;
using namespace util;
using namespace std;
using namespace llvm_util;

#define LLVM_ARGS_PREFIX ""
#define ARGS_SRC_TGT
#define ARGS_REFINEMENT
#include "llvm_util/cmd_args_list.h"

namespace {
llvm::cl::OptionCategory mutatorArgs("Mutator options");

llvm::cl::opt<string> testfile(llvm::cl::Positional,
                               llvm::cl::desc("<inputTestFile>"),
                               llvm::cl::Required,
                               llvm::cl::value_desc("filename"),
                               llvm::cl::cat(mutatorArgs));

}; // namespace

std::shared_ptr<llvm::Module> openInputFile(const string &inputFile,
                                            llvm::LLVMContext &context) {
  llvm::ExitOnError ExitOnErr;
  auto MB =
      ExitOnErr(errorOrToExpected(llvm::MemoryBuffer::getFile(inputFile)));
  llvm::SMDiagnostic Diag;
  auto pm = getLazyIRModule(std::move(MB), Diag, context,
                            /*ShouldLazyLoadMetadata=*/true);
  if (!pm) {
    Diag.print("", llvm::errs(), false);
    return nullptr;
  }
  ExitOnErr(pm->materializeAll());
  return std::move(pm);
}

void handle(std::shared_ptr<llvm::Module> ptr);

int main(int argc, char **argv) {
  llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);
  llvm::PrettyStackTraceProgram X(argc, argv);
  llvm::EnableDebugBuffering = true;
  llvm::llvm_shutdown_obj llvm_shutdown; // Call llvm_shutdown() on exit.

  std::string Usage =
      R"EOF(Alive2 stand-alone LLVM test mutator:
version )EOF";
  Usage += alive_version;

  // llvm::cl::HideUnrelatedOptions(alive_cmdargs);
  llvm::cl::HideUnrelatedOptions(mutatorArgs);
  llvm::cl::ParseCommandLineOptions(argc, argv, Usage);
  llvm::LLVMContext context;
  std::ofstream nout("/dev/null");
  out = &nout;
  report_dir_created = false;

  std::shared_ptr<llvm::Module> pm = openInputFile(testfile, context);
  handle(pm);
  return 0;
}

llvm::Constant* updateIntegerSize(llvm::Instruction* parent,llvm::ConstantInt* constInt,llvm::IntegerType* newTy){
  return llvm::ConstantInt::get(newTy,constInt->getValue());  
}

llvm::BinaryOperator* updateIntegerSize(llvm::Instruction* parent,llvm::BinaryOperator* oper, llvm::IntegerType* newTy){
  return nullptr;
}

llvm::Instruction* updateIntegerSize(llvm::Instruction* parent, llvm::Instruction* inst, llvm::IntegerType* newTy){
  return nullptr;
}

llvm::IntegerType* getNewIntegerTy(llvm::LLVMContext& context){
  return llvm::IntegerType::get(context,1+Random::getRandomUnsigned()%128);
}

void handle(std::shared_ptr<llvm::Module> ptr) {

  llvm::Function* func=ptr->getFunction("_Z3fn1s");
  llvm::BasicBlock* block=&*func->begin();
  auto iit=block->begin();
  llvm::LLVMContext& context=ptr->getContext();
  for(size_t i=0;i<7;++i,++iit);
  //handle use-def chain
  llvm::Value* val=updateIntegerSize(nullptr,&*iit,getNewIntegerTy(context));
  //handle def-use chain
  
}
