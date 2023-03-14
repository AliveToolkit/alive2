// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "cache/cache.h"
#include "ir/type.h"
#include "llvm_util/llvm2alive.h"
#include "llvm_util/utils.h"
#include "smt/expr.h"
#include "smt/smt.h"
#include "smt/solver.h"
#include "tools/transform.h"
#include "util/version.h"

#include "llvm/ADT/StringExtras.h"
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
#include "llvm/TargetParser/Triple.h"

#include <fstream>
#include <iostream>
#include <utility>

using namespace IR;
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

optional<smt::smt_initializer> smt_init;
unique_ptr<Cache> cache;

void execFunction(llvm::Function &F, llvm::TargetLibraryInfoWrapperPass &TLI,
                  unsigned &successCount, unsigned &errorCount) {
  auto Func = llvm2alive(F, TLI.getTLI(F), true);
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
  t.src = std::move(*Func);
  t.tgt = *llvm2alive(F, TLI.getTLI(F), false);
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

  auto error = [&](const Result &r) {
    if (r.isSat() || r.isUnsat())
      return false;

    if (r.isInvalid()) {
      cout << "ERROR: invalid expression\n";
    } else if (r.isError()) {
      cout << "ERROR: Error in SMT solver: " << r.getReason() << '\n';
    } else if (r.isTimeout()) {
      cout << "ERROR: SMT solver timedout\n";
    } else if (r.isSkip()) {
      cout << "ERROR: SMT queries disabled";
    } else {
      UNREACHABLE();
    }
    ++errorCount;
    return true;
  };

  smt_init->reset();
  try {
    auto p = verifier.exec();
    auto &state = *p.first;
    const auto &fn = t.src;
    const auto *bb = &fn.getFirstBB();

    Solver s(true);

    for (auto &[var, val] : state.getValues()) {
      auto &name = var->getName();
      auto *i = dynamic_cast<const Instr*>(var);
      if (!i || &fn.bbOf(*i) != bb)
        continue;

      if (auto *jmp = dynamic_cast<const JumpInstr*>(var)) {
        bool jumped = false;
        expr cond;
        for (auto &dst : jmp->targets()) {
          cond = state.getJumpCond(*bb, dst);

          SolverPush push(s);
          s.add(cond);
          auto r = s.check();
          if (error(r))
            return;

          if ((jumped = r.isSat())) {
            cout << "  >> Jump to " << dst.getName() << "\n\n";
            bb = &dst;
            break;
          }
        }

        if (jumped) {
          s.add(cond);
          continue;
        } else {
          cout << "UB triggered on " << name << "\n\n";
          ++successCount;
          return;
        }
      }

      if (dynamic_cast<const Return*>(var)) {
        const auto &ret = state.returnVal();
        s.add(ret.domain);
        auto r = s.check();
        if (error(r))
            return;
        cout << "Returned: ";
        print_model_val(cout, state, r.getModel(), var, fn.getType(), ret.val);
        cout << "\n\n";
        ++successCount;
        return;
      }

      s.add(val.domain);
      auto r = s.check();
      if (error(r))
        return;

      if (r.isUnsat()) {
        cout << *var << " = UB triggered!\n\n";
        ++successCount;
        return;
      }

      if (name[0] != '%')
        continue;

      cout << *var << " = ";
      print_model_val(cout, state, r.getModel(), var, var->getType(), val.val);
      cout << '\n';
      continue;
    }
  } catch (const AliveException &e) {
    cout << "ERROR: " << e.msg << '\n';
    ++errorCount;
    return;
  }
  ++successCount;
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
