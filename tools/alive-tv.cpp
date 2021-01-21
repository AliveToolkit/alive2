// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "llvm_util/llvm2alive.h"
#include "ir/memory.h"
#include "smt/smt.h"
#include "tools/transform.h"
#include "util/config.h"
#include "util/version.h"

#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/Triple.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/InitializePasses.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/CommandLine.h"
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

static llvm::cl::OptionCategory opt_alive("Alive options");

static llvm::cl::opt<string>
opt_file1(llvm::cl::Positional, llvm::cl::desc("first_bitcode_file"),
    llvm::cl::Required, llvm::cl::value_desc("filename"),
    llvm::cl::cat(opt_alive));

static llvm::cl::opt<string>
opt_file2(llvm::cl::Positional, llvm::cl::desc("[second_bitcode_file]"),
    llvm::cl::Optional, llvm::cl::value_desc("filename"),
    llvm::cl::cat(opt_alive));

static llvm::cl::opt<bool> opt_disable_undef("disable-undef-input",
    llvm::cl::init(false), llvm::cl::cat(opt_alive),
    llvm::cl::desc("Assume inputs are not undef (default=false)"));

static llvm::cl::opt<bool> opt_disable_poison("disable-poison-input",
    llvm::cl::init(false), llvm::cl::cat(opt_alive),
    llvm::cl::desc("Assume inputs are not poison (default=false)"));

static llvm::cl::opt<size_t> opt_max_offset_in_bits(
    "max-offset-in-bits", llvm::cl::init(64), llvm::cl::cat(opt_alive),
    llvm::cl::desc("Upper bound for the maximum pointer offset in bits.  Note "
                   "that this may impact correctness, if values involved in "
                   "offset computations exceed the maximum."));

static llvm::cl::opt<bool> opt_se_verbose(
    "se-verbose", llvm::cl::desc("Symbolic execution verbose mode"),
     llvm::cl::cat(opt_alive), llvm::cl::init(false));

static llvm::cl::opt<unsigned> opt_smt_to(
    "smt-to", llvm::cl::desc("Timeout for SMT queries (default=1000)"),
    llvm::cl::init(1000), llvm::cl::value_desc("ms"), llvm::cl::cat(opt_alive));

static llvm::cl::opt<unsigned> opt_smt_random_seed(
    "smt-random-seed",
    llvm::cl::desc("Random seed for the SMT solver (default=0)"),
    llvm::cl::init(0), llvm::cl::cat(opt_alive));

static llvm::cl::opt<bool> opt_smt_verbose(
    "smt-verbose", llvm::cl::desc("SMT verbose mode"),
    llvm::cl::cat(opt_alive), llvm::cl::init(false));

static llvm::cl::opt<bool> opt_smt_log(
    "smt-log", llvm::cl::desc("Log interactions with the SMT solver"),
    llvm::cl::cat(opt_alive), llvm::cl::init(false));

static llvm::cl::opt<bool> opt_smt_skip(
    "skip-smt", llvm::cl::desc("Skip all SMT queries"),
    llvm::cl::cat(opt_alive), llvm::cl::init(false));

static llvm::cl::list<std::string> opt_funcs(
    "func",
    llvm::cl::desc("Specify the name of a function to verify (without @)"),
    llvm::cl::ZeroOrMore, llvm::cl::value_desc("function name"),
    llvm::cl::cat(opt_alive));

static llvm::cl::opt<std::string> opt_src_fn(
    "src-fn", llvm::cl::desc("Name of src function (without @)"),
    llvm::cl::cat(opt_alive), llvm::cl::init("src"));

static llvm::cl::opt<std::string> opt_tgt_fn(
    "tgt-fn", llvm::cl::desc("Name of tgt function (without @)"),
    llvm::cl::cat(opt_alive), llvm::cl::init("tgt"));

static llvm::cl::opt<unsigned> opt_src_unrolling_factor(
    "src-unroll",
    llvm::cl::desc("Unrolling factor for src function (default=0)"),
    llvm::cl::cat(opt_alive), llvm::cl::init(0));

static llvm::cl::opt<unsigned> opt_tgt_unrolling_factor(
    "tgt-unroll",
    llvm::cl::desc("Unrolling factor for tgt function (default=0)"),
    llvm::cl::cat(opt_alive), llvm::cl::init(0));

static llvm::cl::opt<bool> opt_tactic_verbose(
    "tactic-verbose", llvm::cl::desc("SMT Tactic verbose mode"),
    llvm::cl::cat(opt_alive), llvm::cl::init(false));

static llvm::cl::opt<bool> opt_debug(
    "dbg", llvm::cl::desc("Print debugging info"),
    llvm::cl::cat(opt_alive), llvm::cl::init(false));

static llvm::cl::opt<bool> opt_print_dot(
    "dot",
    llvm::cl::desc("Alive: print .dot files of each function"),
    llvm::cl::cat(opt_alive), llvm::cl::init(false));

static llvm::cl::opt<bool> opt_always_verify(
    "always-verify",
    llvm::cl::desc("Alive: verify the pair even if they are syntactically"
                   " equivalent"),
    llvm::cl::cat(opt_alive), llvm::cl::init(false));

static llvm::cl::opt<bool> opt_smt_stats(
    "smt-stats", llvm::cl::desc("Show SMT statistics"),
    llvm::cl::cat(opt_alive), llvm::cl::init(false));

static llvm::cl::opt<bool> opt_alias_stats(
    "alias-stats", llvm::cl::desc("Show alias sets statistics"),
    llvm::cl::cat(opt_alive), llvm::cl::init(false));

static llvm::cl::opt<bool> opt_succinct(
    "succinct", llvm::cl::desc("Make the output succinct"),
    llvm::cl::cat(opt_alive), llvm::cl::init(false));

static llvm::cl::opt<unsigned> opt_omit_array_size(
    "omit-array-size",
    llvm::cl::desc("Omit an array initializer if it has elements more than "
                   "this number"),
    llvm::cl::cat(opt_alive), llvm::cl::init(-1));

static llvm::cl::opt<unsigned> opt_max_mem(
     "max-mem", llvm::cl::desc("Max memory (approx)"),
     llvm::cl::cat(opt_alive), llvm::cl::init(1024), llvm::cl::value_desc("MB"));

static llvm::cl::opt<bool> opt_bidirectional("bidirectional",
    llvm::cl::init(false), llvm::cl::cat(opt_alive),
    llvm::cl::desc("Run refinement check in both directions"));

static llvm::cl::opt<string> opt_outputfile("o",
    llvm::cl::init(""), llvm::cl::cat(opt_alive),
    llvm::cl::desc("Specify output filename"));

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

static void compareFunctions(llvm::Function &F1, llvm::Function &F2,
                             llvm::Triple &targetTriple, unsigned &goodCount,
                             unsigned &badCount, unsigned &errorCount) {
  TransformPrintOpts print_opts;

  auto Func1 = llvm2alive(F1, llvm::TargetLibraryInfoWrapperPass(targetTriple)
                                    .getTLI(F1));
  if (!Func1) {
    cerr << "ERROR: Could not translate '" << F1.getName().str()
         << "' to Alive IR\n";
    ++errorCount;
    return;
  }

  auto Func2 = llvm2alive(F2, llvm::TargetLibraryInfoWrapperPass(targetTriple)
                                    .getTLI(F2), Func1->getGlobalVarNames());
  if (!Func2) {
    cerr << "ERROR: Could not translate '" << F2.getName().str()
         << "' to Alive IR\n";
    ++errorCount;
    return;
  }

  if (opt_print_dot) {
    Func1->writeDot("src");
    Func2->writeDot("tgt");
  }

  if (!opt_always_verify) {
    stringstream ss1, ss2;
    Func1->print(ss1);
    Func2->print(ss2);
    if (ss1.str() == ss2.str()) {
      if (!opt_succinct)
        Transform{"", move(*Func1), move(*Func2)}.print(cout, print_opts);
      cout << "Transformation seems to be correct! (syntactically equal)\n\n";
      ++goodCount;
      return;
    }
  }

  smt_init->reset();
  Transform t;
  t.src = move(*Func1);
  t.tgt = move(*Func2);
  t.preprocess();
  TransformVerify verifier(t, false);
  if (!opt_succinct)
    t.print(cout, print_opts);

  {
    auto types = verifier.getTypings();
    if (!types) {
      cerr << "Transformation doesn't verify!\n"
              "ERROR: program doesn't type check!\n\n";
      ++errorCount;
      return;
    }
    assert(types.hasSingleTyping());
  }

  Errors errs = verifier.verify();
  bool result(errs);
  if (result) {
    if (errs.isUnsound()) {
      cout << "Transformation doesn't verify!\n";
      if (!opt_succinct)
        cout << errs << endl;
      ++badCount;
    } else {
      cerr << errs << endl;
      ++errorCount;
    }
  } else {
    cout << "Transformation seems to be correct!\n\n";
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
      cout << "Reverse transformation doesn't verify!\n" << errs2 << endl;
    } else {
      cout << "Reverse transformation seems to be correct!\n\n";
      if (!result)
        cout << "These functions are equivalent.\n\n";
    }
  }
}

static void optimizeModule(llvm::Module *M) {
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

  llvm::FunctionPassManager FPM =
    PB.buildFunctionSimplificationPipeline(
      llvm::PassBuilder::OptimizationLevel::O2, llvm::ThinOrFullLTOPhase::None);
  llvm::ModulePassManager MPM;
  MPM.addPass(createModuleToFunctionPassAdaptor(std::move(FPM)));
  MPM.run(*M, MAM);
}

static llvm::Function *findFunction(llvm::Module &M, const std::string FName) {
  for (auto &F : M) {
    if (F.isDeclaration())
      continue;
    if (FName.compare(F.getName()) != 0)
      continue;
    return &F;
  }
  return 0;
}

static ofstream OutFile;

int main(int argc, char **argv) {
  llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);
  llvm::PrettyStackTraceProgram X(argc, argv);
  llvm::EnableDebugBuffering = true;
  llvm::llvm_shutdown_obj llvm_shutdown; // Call llvm_shutdown() on exit.
  llvm::LLVMContext Context;

  std::string Usage =
      R"EOF(Alive2 stand-alone translation validator:
version )EOF";
  Usage += alive_version;
  Usage += R"EOF(
see alive-tv --version  for LLVM version info,

This program takes either one or two LLVM IR files files as
command-line arguments. Both .bc and .ll files are supported.

If two files are provided, alive-tv checks that functions in the
second file refine functions in the first file, matching up functions
by name. Functions not found in both files are ignored. It is an error
for a function to be found in both files unless they have the same
signature.

If one file is provided, there are two possibilities. If the file
contains a function called "src" and also a function called "tgt",
then alive-tv will determine whether src is refined by tgt. It is an
error if src and tgt do not have the same signature. Otherwise,
alive-tv will optimize the entire module using an optimization
pipeline similar to -O2, and then verify that functions in the
optimized module refine those in the original one. This provides a
convenient way to demonstrate an existing optimizer bug.
)EOF";

  llvm::cl::HideUnrelatedOptions(opt_alive);
  llvm::cl::ParseCommandLineOptions(argc, argv, Usage);

  smt::solver_print_queries(opt_smt_verbose);
  smt::solver_tactic_verbose(opt_tactic_verbose);
  smt::set_query_timeout(to_string(opt_smt_to));
  smt::set_random_seed(to_string(opt_smt_random_seed));
  smt::set_memory_limit((uint64_t)opt_max_mem * 1024 * 1024);
  config::skip_smt = opt_smt_skip;
  config::symexec_print_each_value = opt_se_verbose;
  config::disable_undef_input = opt_disable_undef;
  config::disable_poison_input = opt_disable_poison;
  config::debug = opt_debug;
  config::src_unroll_cnt = opt_src_unrolling_factor;
  config::tgt_unroll_cnt = opt_tgt_unrolling_factor;
  config::max_offset_bits = opt_max_offset_in_bits;

  if (opt_smt_log)
    smt::start_logging();

  // optionally, redirect cout and cerr to user-specified file
  if (!opt_outputfile.empty()) {
    OutFile.open(opt_outputfile);
    std::cout.rdbuf(OutFile.rdbuf());
  }

  auto M1 = openInputFile(Context, opt_file1);
  if (!M1.get()) {
    cerr << "Could not read bitcode from '" << opt_file1 << "'\n";
    return -1;
  }

  auto &DL = M1.get()->getDataLayout();
  auto targetTriple = llvm::Triple(M1.get()->getTargetTriple());

  llvm_util::initializer llvm_util_init(cerr, DL);
  omit_array_size = opt_omit_array_size;
  smt_init.emplace();

  unsigned goodCount = 0, badCount = 0, errorCount = 0;

  unique_ptr<llvm::Module> M2;
  if (opt_file2.empty()) {
    auto SRC = findFunction(*M1, opt_src_fn);
    auto TGT = findFunction(*M1, opt_tgt_fn);
    if (SRC && TGT) {
      compareFunctions(*SRC, *TGT, targetTriple, goodCount, badCount,
                       errorCount);
      goto end;
    } else {
      M2 = CloneModule(*M1);
      optimizeModule(M2.get());
    }
  } else {
    M2 = openInputFile(Context, opt_file2);
    if (!M2.get()) {
      cerr << "Could not read bitcode from '" << opt_file2 << "'\n";
      return -1;
    }
  }

  if (M1.get()->getTargetTriple() != M2.get()->getTargetTriple()) {
    cerr << "Modules have different target triples\n";
    return -1;
  }

  {
  set<string> funcNames(opt_funcs.begin(), opt_funcs.end());

  // FIXME: quadratic, may not be suitable for very large modules
  // emitted by opt-fuzz
  for (auto &F1 : *M1.get()) {
    if (F1.isDeclaration())
      continue;
    for (auto &F2 : *M2.get()) {
      if (F2.isDeclaration() ||
          F1.getName() != F2.getName())
        continue;
      if (!funcNames.empty() && funcNames.count(F1.getName().str()) == 0)
        continue;
      compareFunctions(F1, F2, targetTriple, goodCount, badCount, errorCount);
      break;
    }
  }

  cout << "Summary:\n"
          "  " << goodCount << " correct transformations\n"
          "  " << badCount << " incorrect transformations\n"
          "  " << errorCount << " Alive2 errors\n";
  }

end:
  if (opt_smt_stats)
    smt::solver_print_stats(cout);

  smt_init.reset();

  if (opt_alias_stats)
    IR::Memory::printAliasStats(cout);

  return errorCount > 0;
}
