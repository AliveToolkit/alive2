// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "llvm/ADT/StringExtras.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/ToolOutputFile.h"

#include "ir/function.h"
#include "llvm_util/utils.h"
#include "smt/smt.h"
#include "smt/solver.h"
#include "tools/alive_parser.h"
#include "util/config.h"
#include "util/file.h"
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string_view>
#include <vector>

using namespace IR;
using namespace tools;
using namespace util;
using namespace std;
using namespace llvm_util;

static llvm::cl::OptionCategory opt_alive("Alive options");

static llvm::cl::opt<std::string>
opt_file1(llvm::cl::Positional, llvm::cl::desc("first_bitcode_file"),
    llvm::cl::Required, llvm::cl::value_desc("filename"),
    llvm::cl::cat(opt_alive));

static llvm::cl::opt<std::string>
opt_file2(llvm::cl::Positional, llvm::cl::desc("second_bitcode_file"),
    llvm::cl::Required, llvm::cl::value_desc("filename"),
    llvm::cl::cat(opt_alive));

static llvm::cl::opt<bool> opt_disable_undef("disable-undef-input",
    llvm::cl::init(false), llvm::cl::cat(opt_alive),
    llvm::cl::desc("Alive: Assume inputs are not undef (default=false)"));

static llvm::cl::opt<bool> opt_disable_poison("disable-poison-input",
    llvm::cl::init(false), llvm::cl::cat(opt_alive),
    llvm::cl::desc("Alive: Assume inputs are not poison (default=false)"));

static llvm::cl::opt<unsigned> opt_smt_to(
  "tv-smt-to", llvm::cl::desc("Alive: timeout for SMT queries (default=1000)"),
  llvm::cl::init(1000), llvm::cl::value_desc("ms"), llvm::cl::cat(opt_alive));

static llvm::ExitOnError ExitOnErr;

// adapted from llvm-dis.cpp
static std::unique_ptr<llvm::Module> openInputFile(llvm::LLVMContext &Context,
                                                   std::string InputFilename) {
  std::unique_ptr<llvm::MemoryBuffer> MB =
    ExitOnErr(errorOrToExpected(llvm::MemoryBuffer::getFile(InputFilename)));
  std::unique_ptr<llvm::Module> M =
    ExitOnErr(getOwningLazyBitcodeModule(std::move(MB), Context,
                                         /*ShouldLazyLoadMetadata=*/true));
  ExitOnErr(M->materializeAll());
  return M;
}

static llvm::Function *getSingleFunction(llvm::Module *M) {
  llvm::Function *Ret = 0;
  for (auto &F : *M) {
    if (F.getInstructionCount() == 0)
      continue;
    if (!Ret)
      Ret = &F;
    else
      llvm::report_fatal_error("Error, bitcode contains multiple functions");
  }
  if (!Ret)
      llvm::report_fatal_error("Error, bitcode contains no functions");
  return Ret;
}

int main(int argc, char **argv) {
  llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);
  llvm::PrettyStackTraceProgram X(argc, argv);
  llvm::EnableDebugBuffering = true;
  llvm::llvm_shutdown_obj llvm_shutdown;  // Call llvm_shutdown() on exit.
  llvm::LLVMContext Context;

  llvm::cl::ParseCommandLineOptions(argc, argv,
                                    "Alive2 stand-alone translation validator\n");

  auto M1 = openInputFile(Context, opt_file1);
  if (!M1.get())
    llvm::report_fatal_error("Could not read bitcode from '" + opt_file1 + "'");
  auto F1 = getSingleFunction(M1.get());
  auto Func1 = llvm2alive(*F1);

  auto M2 = openInputFile(Context, opt_file2);
  if (!M2.get())
    llvm::report_fatal_error("Could not read bitcode from '" + opt_file2 + "'");
  auto F2 = getSingleFunction(M2.get());
  auto Func2 = llvm2alive(*F2);

  // TODO check that function signatures match before proceeding?

  smt::solver_print_queries(false);
  smt::solver_tactic_verbose(false);
  smt::set_query_timeout(to_string(opt_smt_to));
  smt::set_memory_limit(1024 * 1024 * 1024);
  //config::skip_smt = opt_smt_skip;
  config::symexec_print_each_value = true;
  config::disable_undef_input = opt_disable_undef;
  config::disable_poison_input = opt_disable_poison;
  
  llvm_util::initializer llvm_util_init(cerr);
  smt::smt_initializer smt_init;
  TransformPrintOpts print_opts;

  Transform t;
  t.src = move(*Func1);
  t.tgt = move(*Func2);
  TransformVerify verifier(t, false);
  t.print(std::cout, print_opts);

  if (Errors errs = verifier.verify()) {
    cerr << "Transformation doesn't verify!\n" << errs << endl;
    return 1;
  } else {
    cerr << "Transformation seems to be correct!\n\n";
  }

  return 0;
}
