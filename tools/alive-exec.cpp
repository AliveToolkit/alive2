// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "llvm_util/llvm2alive.h"
#include "smt/expr.h"
#include "smt/smt.h"
#include "smt/solver.h"
#include "tools/transform.h"
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

#define LLVM_ARGS_PREFIX ""
#include "llvm_util/cmd_args_list.h"

namespace {

llvm::cl::opt<string> opt_file(llvm::cl::Positional,
  llvm::cl::desc("bitcode_file"), llvm::cl::Required,
  llvm::cl::value_desc("filename"), llvm::cl::cat(alive_cmdargs));


llvm::ExitOnError ExitOnErr;

// adapted from llvm-dis.cpp
std::unique_ptr<llvm::Module> openInputFile(llvm::LLVMContext &Context,
                                            const string &InputFilename) {
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

optional<smt::smt_initializer> smt_init;

void execFunction(llvm::Function &F, llvm::TargetLibraryInfoWrapperPass &TLI,
                  unsigned &successCount, unsigned &errorCount) {
  auto Func = llvm2alive(F, TLI.getTLI(F));
  if (!Func) {
    cerr << "ERROR: Could not translate '" << F.getName().str()
         << "' to Alive IR\n";
    ++errorCount;
    return;
  }
  if (opt_print_dot) {
    Func->writeDot("");
  }

  Transform t;
  t.src = move(*Func);
  t.tgt = *llvm2alive(F, TLI.getTLI(F));
  t.preprocess();
  TransformVerify verifier(t, false);
  if (!opt_quiet)
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

    auto [ret_val, ret_domain, ret_undefs] = state.returnVal();
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
}

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

  llvm::cl::HideUnrelatedOptions(alive_cmdargs);
  llvm::cl::ParseCommandLineOptions(argc, argv, Usage);

  auto M = openInputFile(Context, opt_file);
  if (!M.get()) {
    cerr << "Could not read bitcode from '" << opt_file << "'\n";
    return -1;
  }

#define ARGS_MODULE_VAR M
# include "llvm_util/cmd_args_def.h"

  auto &DL = M.get()->getDataLayout();
  llvm::Triple targetTriple(M.get()->getTargetTriple());
  llvm::TargetLibraryInfoWrapperPass TLI(targetTriple);

  llvm_util::initializer llvm_util_init(cerr, DL);
  smt_init.emplace();

  unsigned successCount = 0, errorCount = 0;

  for (auto &F : *M.get()) {
    if (F.isDeclaration())
      continue;
    if (!func_names.empty() && !func_names.count(F.getName().str()))
      continue;
    execFunction(F, TLI, successCount, errorCount);
  }

  cout << "Summary:\n"
          "  " << successCount << " functions interpreted successfully\n"
          "  " << errorCount << " Alive2 errors\n";

  if (opt_smt_stats)
    smt::solver_print_stats(cout);

  smt_init.reset();

  if (opt_alias_stats)
    IR::Memory::printAliasStats(cout);

  return errorCount > 0;
}
