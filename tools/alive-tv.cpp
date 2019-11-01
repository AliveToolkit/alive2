// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "llvm_util/llvm2alive.h"
#include "smt/smt.h"
#include "tools/transform.h"
#include "util/config.h"

#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/Triple.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Transforms/Utils/FunctionComparator.h"

#include <iostream>
#include <utility>

using namespace tools;
using namespace util;
using namespace std;
using namespace llvm_util;

static llvm::cl::OptionCategory opt_alive("Alive options");

static llvm::cl::opt<string>
opt_file1(llvm::cl::Positional, llvm::cl::desc("first_bitcode_file"),
    llvm::cl::Required, llvm::cl::value_desc("filename"),
    llvm::cl::cat(opt_alive));

static llvm::cl::opt<string>
opt_file2(llvm::cl::Positional, llvm::cl::desc("second_bitcode_file"),
    llvm::cl::Required, llvm::cl::value_desc("filename"),
    llvm::cl::cat(opt_alive));

static llvm::cl::opt<bool> opt_disable_undef("disable-undef-input",
    llvm::cl::init(false), llvm::cl::cat(opt_alive),
    llvm::cl::desc("Alive: Assume inputs are not undef (default=false)"));

static llvm::cl::opt<bool> opt_disable_poison("disable-poison-input",
    llvm::cl::init(false), llvm::cl::cat(opt_alive),
    llvm::cl::desc("Alive: Assume inputs are not poison (default=false)"));

static llvm::cl::opt<bool> opt_se_verbose(
    "se-verbose", llvm::cl::desc("Alive: symbolic execution verbose mode"),
     llvm::cl::cat(opt_alive), llvm::cl::init(false));

static llvm::cl::opt<unsigned> opt_smt_to(
  "smt-to", llvm::cl::desc("Alive: timeout for SMT queries (default=1000)"),
  llvm::cl::init(1000), llvm::cl::value_desc("ms"), llvm::cl::cat(opt_alive));

static llvm::cl::opt<bool> opt_smt_verbose(
    "smt-verbose", llvm::cl::desc("Alive: SMT verbose mode"),
    llvm::cl::cat(opt_alive), llvm::cl::init(false));

static llvm::cl::opt<bool> opt_smt_stats(
    "smt-stats", llvm::cl::desc("Alive: show SMT statistics"),
    llvm::cl::cat(opt_alive), llvm::cl::init(false));

static llvm::cl::opt<bool> opt_bidirectional("bidirectional",
    llvm::cl::init(false), llvm::cl::cat(opt_alive),
    llvm::cl::desc("Alive: Run refinement check in both directions"));

static llvm::ExitOnError ExitOnErr;

// adapted from llvm-dis.cpp
static std::unique_ptr<llvm::Module> openInputFile(llvm::LLVMContext &Context,
                                                   string InputFilename) {
  auto MB =
    ExitOnErr(errorOrToExpected(llvm::MemoryBuffer::getFile(InputFilename)));
  llvm::SMDiagnostic Diag;
  auto M = getLazyIRModule(move(MB), Diag, Context,
                           /*ShouldLazyLoadMetadata=*/true);
  if (!M) {
    Diag.print("", llvm::errs(), false);
    return 0;
  }
  ExitOnErr(M->materializeAll());
  return M;
}

static optional<smt::smt_initializer> smt_init;

static bool compareFunctions(llvm::Function &F1, llvm::Function &F2,
                             llvm::Triple &targetTriple, unsigned &goodCount,
                             unsigned &badCount, unsigned &errorCount) {
  llvm::GlobalNumberState GN;
  llvm::FunctionComparator FCmp(&F1, &F2, &GN);
  if (!FCmp.compare()) {
    cerr << "Syntactically equivalent LLVM functions.\n";
    ++goodCount;
    return false;
  }

  TransformPrintOpts print_opts;

  auto Func1 = llvm2alive(F1, llvm::TargetLibraryInfoWrapperPass(targetTriple)
                                    .getTLI(F1));
  if (!Func1) {
    cerr << "ERROR: Could not translate '" << F1.getName().str()
         << "' to Alive IR\n";
    ++errorCount;
    return true;
  }

  auto Func2 = llvm2alive(F2, llvm::TargetLibraryInfoWrapperPass(targetTriple)
                                    .getTLI(F2));
  if (!Func2) {
    cerr << "ERROR: Could not translate '" << F2.getName().str()
         << "' to Alive IR\n";
    ++errorCount;
    return true;
  }

  smt_init->reset();
  Transform t;
  t.src = move(*Func1);
  t.tgt = move(*Func2);
  TransformVerify verifier(t, false);
  t.print(cout, print_opts);

  {
    auto types = verifier.getTypings();
    if (!types) {
      cerr << "Transformation doesn't verify!\n"
              "ERROR: program doesn't type check!\n\n";
      ++errorCount;
      return true;
    }
    assert(types.hasSingleTyping());
  }

  Errors errs = verifier.verify();
  bool result(errs);
  if (result) {
    if (errs.isUnsound()) {
      cerr << "Transformation doesn't verify!\n" << errs << endl;
      ++badCount;
    } else {
      cerr << errs << endl;
      ++errorCount;
    }
  } else {
    cerr << "Transformation seems to be correct!\n\n";
    ++goodCount;
  }

  if (opt_bidirectional) {
    smt_init->reset();
    Transform t2;
    t2.src = move(t.tgt);
    t2.tgt = move(t.src);
    TransformVerify verifier2(t2, false);
    t2.print(cout, print_opts);

    if (Errors errs2 = verifier2.verify()) {
      cerr << "Reverse transformation doesn't verify!\n" << errs2 << endl;
    } else {
      cerr << "Reverse transformation seems to be correct!\n\n";
      if (!result)
        cerr << "These functions are equivalent.\n\n";
    }
  }

  return result;
}

int main(int argc, char **argv) {
  llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);
  llvm::PrettyStackTraceProgram X(argc, argv);
  llvm::EnableDebugBuffering = true;
  llvm::llvm_shutdown_obj llvm_shutdown;  // Call llvm_shutdown() on exit.
  llvm::LLVMContext Context;

  llvm::cl::ParseCommandLineOptions(argc, argv,
                                  "Alive2 stand-alone translation validator\n");

  smt::solver_print_queries(opt_smt_verbose);
  smt::solver_tactic_verbose(false);
  smt::set_query_timeout(to_string(opt_smt_to));
  smt::set_memory_limit(1024 * 1024 * 1024);
  //config::skip_smt = opt_smt_skip;
  config::symexec_print_each_value = opt_se_verbose;
  config::disable_undef_input = opt_disable_undef;
  config::disable_poison_input = opt_disable_poison;

  auto M1 = openInputFile(Context, opt_file1);
  if (!M1.get())
    llvm::report_fatal_error(
      "Could not read bitcode from '" + opt_file1 + "'");

  auto M2 = openInputFile(Context, opt_file2);
  if (!M2.get())
    llvm::report_fatal_error(
      "Could not read bitcode from '" + opt_file2 + "'");

  if (M1.get()->getTargetTriple() != M2.get()->getTargetTriple())
    llvm::report_fatal_error("Modules have different target triple");

  auto &DL = M1.get()->getDataLayout();
  auto targetTriple = llvm::Triple(M1.get()->getTargetTriple());

  llvm_util::initializer llvm_util_init(cerr, DL);
  smt_init.emplace();

  bool result = false;
  unsigned goodCount = 0, badCount = 0, errorCount = 0;
  // FIXME: quadratic, may not be suitable for very large modules
  // emitted by opt-fuzz
  for (auto &F1 : *M1.get()) {
    if (F1.isDeclaration())
      continue;
    for (auto &F2 : *M2.get()) {
      if (F2.isDeclaration() ||
          F1.getName() != F2.getName())
        continue;
      result |= compareFunctions(F1, F2, targetTriple, goodCount, badCount,
                                 errorCount);
      break;
    }
  }

  cerr << "Summary:\n"
          "  " << goodCount << " correct transformations\n"
          "  " << badCount << " incorrect transformations\n"
          "  " << errorCount << " errors\n";

  if (opt_smt_stats)
    smt::solver_print_stats(cerr);

  smt_init.reset();

  return result;
}
