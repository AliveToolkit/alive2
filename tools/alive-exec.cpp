// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "llvm_util/llvm2alive.h"
#include "ir/memory.h"
#include "smt/expr.h"
#include "smt/smt.h"
#include "smt/solver.h"
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
#include <utility>

using namespace llvm_util;
using namespace smt;
using namespace tools;
using namespace util;
using namespace std;

static llvm::cl::OptionCategory opt_alive("Alive options");

static llvm::cl::opt<string>
opt_file(llvm::cl::Positional, llvm::cl::desc("bitcode_file"),
    llvm::cl::Required, llvm::cl::value_desc("filename"),
    llvm::cl::cat(opt_alive));

static llvm::cl::opt<bool> opt_disable_undef("disable-undef-input",
    llvm::cl::init(false), llvm::cl::cat(opt_alive),
    llvm::cl::desc("Assume inputs are not undef (default=false)"));

static llvm::cl::opt<bool> opt_disable_poison("disable-poison-input",
    llvm::cl::init(false), llvm::cl::cat(opt_alive),
    llvm::cl::desc("Assume inputs are not poison (default=false)"));

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
    "funcs",
    llvm::cl::desc("Specify the name of one or more function to execute (without @)"),
    llvm::cl::ZeroOrMore, llvm::cl::value_desc("function name"),
    llvm::cl::CommaSeparated, llvm::cl::cat(opt_alive));

static llvm::cl::opt<unsigned> opt_unrolling_factor(
    "unroll",
    llvm::cl::desc("Unrolling factor (default=0)"),
    llvm::cl::cat(opt_alive), llvm::cl::init(0));

static llvm::cl::opt<bool> opt_tactic_verbose(
    "tactic-verbose", llvm::cl::desc("SMT Tactic verbose mode"),
    llvm::cl::cat(opt_alive), llvm::cl::init(false));

static llvm::cl::opt<bool> opt_debug(
    "dbg", llvm::cl::desc("Print debugging info"),
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

static llvm::cl::opt<bool> opt_io_nobuiltin(
    "io-nobuiltin",
    llvm::cl::desc("Encode standard I/O functions as an unknown function"),
    llvm::cl::cat(opt_alive), llvm::cl::init(false));

static llvm::cl::opt<unsigned> opt_max_mem(
     "max-mem", llvm::cl::desc("Max memory (approx)"),
     llvm::cl::cat(opt_alive), llvm::cl::init(1024), llvm::cl::value_desc("MB"));

static llvm::cl::opt<string> opt_outputfile("o",
    llvm::cl::init(""), llvm::cl::cat(opt_alive),
    llvm::cl::desc("Specify output filename"));

static llvm::cl::opt<bool> opt_print_cfg(
    "print-cfg",
    llvm::cl::desc("Print CFG dot source to stdout"),
    llvm::cl::cat(opt_alive), llvm::cl::init(false));

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

static void execFunction(llvm::Function &F, llvm::Triple &triple,
                         unsigned &successCount, unsigned &errorCount) {
  auto Func = llvm2alive(F,
                         llvm::TargetLibraryInfoWrapperPass(triple).getTLI(F));
  if (!Func) {
    cerr << "ERROR: Could not translate '" << F.getName().str()
         << "' to Alive IR\n";
    ++errorCount;
    return;
  }
  if (opt_print_cfg) {
    IR::CFG cfg(*Func);
    cfg.printDot(cout);
  }
  Func->unroll(opt_unrolling_factor);

  Transform t;
  t.src = move(*Func);
  t.tgt = *llvm2alive(F,
                      llvm::TargetLibraryInfoWrapperPass(triple).getTLI(F));
  t.preprocess();
  TransformVerify verifier(t, false);
  if (!opt_succinct)
    t.src.print(cout << "\n----------------------------------------\n");

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

  smt_init->reset();
  try {
    auto p = verifier.exec();
    auto &state = *p.first;

    auto ret_domain = state.returnDomain()();
    auto [ret_val, ret_undefs] = state.returnVal();
    auto ret = expr::mkVar("ret_val", ret_val.value);
    auto ret_np = expr::mkVar("ret_np", ret_val.non_poison);

    Solver s;
    s.add(ret_domain);
    auto r = s.check();
    if (r.isUnsat()) {
      cout << "ERROR: Function doesn't reach a return statement\n";
      ++errorCount;
      return;
    }
    if (r.isInvalid()) {
      cout << "ERROR: invalid expression\n";
      ++errorCount;
      return;
    }
    if (r.isError()) {
      cout << "ERROR: Error in SMT solver: " << r.getReason() << '\n';
      ++errorCount;
      return;
    }
    if (r.isTimeout()) {
      cout << "ERROR: SMT solver timedout\n";
      ++errorCount;
      return;
    }
    if (r.isSkip()) {
      cout << "ERROR: SMT queries disabled";
      ++errorCount;
      return;
    }
    if (r.isSat()) {
      auto &m = r.getModel();
      cout << "Return value: ";
      // TODO: add support for aggregates
      if (m.eval(ret_val.non_poison, true).isFalse()) {
        cout << "poison\n\n";
      } else {
        t.src.getType().printVal(cout, state, m.eval(ret_val.value, true));

        s.block(m);
        if (s.check().isSat()) {
          cout << "\n\nWARNING: There are multiple return values";
        }
        cout << "\n\n";
      }
      ++successCount;
      return;
    }
    UNREACHABLE();

  } catch (const AliveException &e) {
    cout << "ERROR: " << e.msg << '\n';
    ++errorCount;
    return;
  }
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
see alive-exec --version  for LLVM version info,

This program takes an LLVM IR files files as a command-line
argument. Both .bc and .ll files are supported.

If one or more functions are specified (as a comma-separated list)
using the --funcs command line option, alive-exec will attempt to
execute them using Alive2 as an interpreter. There are currently many
restrictions on what kind of functions can be executed: they cannot
take inputs, cannot use memory, cannot depend on undefined behaviors,
and cannot include loops that execute too many iterations.

If no functions are specified on the command line, then alive-exec
will attempt to execute every function in the bitcode file.
)EOF";

  llvm::cl::HideUnrelatedOptions(opt_alive);
  llvm::cl::ParseCommandLineOptions(argc, argv, Usage);

  smt::solver_print_queries(opt_smt_verbose);
  smt::solver_tactic_verbose(opt_tactic_verbose);
  smt::set_query_timeout(to_string(opt_smt_to));
  smt::set_random_seed(to_string(opt_smt_random_seed));
  smt::set_memory_limit((uint64_t)opt_max_mem * 1024 * 1024);
  config::skip_smt = opt_smt_skip;
  config::io_nobuiltin = opt_io_nobuiltin;
  config::symexec_print_each_value = opt_se_verbose;
  config::disable_undef_input = opt_disable_undef;
  config::disable_poison_input = opt_disable_poison;
  config::debug = opt_debug;

  if (opt_smt_log)
    smt::start_logging();

  // optionally, redirect cout and cerr to user-specified file
  if (!opt_outputfile.empty()) {
    OutFile.open(opt_outputfile);
    std::cout.rdbuf(OutFile.rdbuf());
  }

  auto M = openInputFile(Context, opt_file);
  if (!M.get()) {
    cerr << "Could not read bitcode from '" << opt_file << "'\n";
    return -1;
  }

  auto &DL = M.get()->getDataLayout();
  auto targetTriple = llvm::Triple(M.get()->getTargetTriple());

  llvm_util::initializer llvm_util_init(cerr, DL);
  omit_array_size = opt_omit_array_size;
  smt_init.emplace();

  unsigned successCount = 0, errorCount = 0;

  {
  set<string> funcNames(opt_funcs.begin(), opt_funcs.end());

  for (auto &F : *M.get()) {
    if (F.isDeclaration())
      continue;
    if (!funcNames.empty() && funcNames.count(F.getName().str()) == 0)
      continue;
    execFunction(F, targetTriple, successCount, errorCount);
  }

  cout << "Summary:\n"
          "  " << successCount << " functions interpreted successfully\n"
          "  " << errorCount << " Alive2 errors\n";
  }

  if (opt_smt_stats)
    smt::solver_print_stats(cout);

  smt_init.reset();

  if (opt_alias_stats)
    IR::Memory::printAliasStats(cout);

  return errorCount > 0;
}
