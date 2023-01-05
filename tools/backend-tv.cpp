// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "backend_tv/lifter.h"
#include "cache/cache.h"
#include "llvm_util/compare.h"
#include "llvm_util/llvm2alive.h"
#include "llvm_util/llvm_optimizer.h"
#include "llvm_util/utils.h"
#include "smt/smt.h"
#include "tools/transform.h"
#include "util/version.h"

#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/Triple.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/InitializePasses.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Utils/Cloning.h"

#include <fstream>
#include <iostream>
#include <sstream>
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

llvm::cl::opt<string> opt_file(llvm::cl::Positional,
  llvm::cl::desc("bitcode_file"),
  llvm::cl::Required, llvm::cl::value_desc("filename"),
  llvm::cl::cat(alive_cmdargs));

llvm::cl::opt<std::string> opt_fn(LLVM_ARGS_PREFIX "fn",
  llvm::cl::desc("Name of function to verify, without @ (default "
                 "= first function in the module)"),
  llvm::cl::cat(alive_cmdargs));

llvm::cl::opt<bool> opt_optimize_tgt(LLVM_ARGS_PREFIX "optimize-tgt",
  llvm::cl::desc("Optimize lifted code before performing translation "
		 "validation (default=true)"),
  llvm::cl::cat(alive_cmdargs), llvm::cl::init(true));

// FIXME support opt_asm_only and opt_asm_input
  
llvm::cl::opt<bool> opt_asm_only(
    "asm-only",
    llvm::cl::desc("Only generate assembly and exit (default=false)"),
    llvm::cl::init(false), llvm::cl::cat(alive_cmdargs));

llvm::cl::opt<string> opt_asm_input(
    "asm-input",
    llvm::cl::desc("Use the provied file as lifted assembly, instead of "
		   "lifting the LLVM IR. This is only for testing. "
		   "(default=no asm input)"),
    llvm::cl::cat(alive_cmdargs));

llvm::ExitOnError ExitOnErr;

llvm::Function *findFirstFunction(llvm::Module &M) {
  for (auto &F : M) {
    if (F.isDeclaration())
      continue;
    return &F;
  }
  return 0;
}

} // namespace

unique_ptr<Cache> cache;

int main(int argc, char **argv) {
  llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);
  llvm::PrettyStackTraceProgram X(argc, argv);
  llvm::EnableDebugBuffering = true;
  llvm::llvm_shutdown_obj llvm_shutdown; // Call llvm_shutdown() on exit.
  llvm::LLVMContext Context;

  std::string Usage =
      R"EOF(Alive2 stand-alone translation validator for the AArch64 backend:
version )EOF";
  Usage += alive_version;

  llvm::cl::HideUnrelatedOptions(alive_cmdargs);
  llvm::cl::ParseCommandLineOptions(argc, argv, Usage);

  auto M1 = openInputFile(Context, opt_file);
  if (!M1.get()) {
    cerr << "Could not read bitcode from '" << opt_file << "'\n";
    return -1;
  }

#define ARGS_MODULE_VAR M1
# include "llvm_util/cmd_args_def.h"

  // FIXME: For now, we're hardcoding these
  M1.get()->setTargetTriple("aarch64-linux-gnu");
  M1.get()->setDataLayout(
      "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128");

  auto &DL = M1.get()->getDataLayout();
  llvm::Triple targetTriple(M1.get()->getTargetTriple());
  llvm::TargetLibraryInfoWrapperPass TLI(targetTriple);

  llvm_util::initializer llvm_util_init(*out, DL);
  smt::smt_initializer smt_init;
  Verifier verifier(TLI, smt_init, *out);
  verifier.quiet = opt_quiet;
  verifier.always_verify = opt_always_verify;
  verifier.print_dot = opt_print_dot;
  verifier.bidirectional = opt_bidirectional;

  llvm::Function *srcFn;
  if (opt_fn == "")
    srcFn = findFirstFunction(*M1);
  else
    srcFn = findFunction(*M1, opt_fn);
  if (!srcFn) {
    *out << "Fatal error: Couldn't find function to verify\n";
    exit(-1);
  }

  // this has to return a fresh function since it rewrites the
  // signature
  srcFn = lifter::adjustSrc(srcFn);
  
  std::unique_ptr<llvm::Module> M2 = std::make_unique<llvm::Module>("M2", Context);
  M2->setDataLayout(M1.get()->getDataLayout());
  M2->setTargetTriple(M1.get()->getTargetTriple());

  lifter::init();
  llvm::SmallString<1024> Asm;
  auto AsmBuffer = (opt_asm_input != "") ?
    ExitOnErr(llvm::errorOrToExpected(llvm::MemoryBuffer::getFile(opt_asm_input))) :
    lifter::generateAsm(*M1.get(), Asm);

  cout << "\n\nARM Assembly:\n\n";
  for (auto it = AsmBuffer->getBuffer().begin(); it != AsmBuffer->getBuffer().end();
       ++it) {
    cout << *it;
  }
  cout << "-------------\n";

  if (opt_asm_only)
    exit(0);

  auto [F1, F2] = lifter::liftFunc(M1.get(), M2.get(), srcFn, std::move(AsmBuffer));
  
  if (opt_optimize_tgt) {
    auto err = optimize_module(M2.get(), "Oz");
    assert(err.empty());
  }

  verifier.compareFunctions(*F1, *F2);

  *out << "Summary:\n"
          "  " << verifier.num_correct << " correct transformations\n"
          "  " << verifier.num_unsound << " incorrect transformations\n"
          "  " << verifier.num_failed  << " failed-to-prove transformations\n"
          "  " << verifier.num_errors << " Alive2 errors\n";

  if (opt_smt_stats)
    smt::solver_print_stats(*out);

  smt_init.reset();

  if (opt_alias_stats)
    IR::Memory::printAliasStats(*out);

  return verifier.num_errors > 0;
}
