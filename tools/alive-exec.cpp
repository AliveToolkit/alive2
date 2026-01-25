// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "cache/cache.h"
#include "llvm_util/llvm2alive.h"
#include "llvm_util/utils.h"
#include "smt/smt.h"
#include "smt/solver.h"
#include "tools/transform.h"
#include "util/symexec.h"
#include "util/version.h"

#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/InitializePasses.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/Signals.h"
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

StateValue eval(const Result &r, const StateValue &v) {
  auto &m = r.getModel();
  return { m[v.value], m[v.non_poison] };
}

StateValue exec(llvm::Function &F, llvm::TargetLibraryInfoWrapperPass &TLI) {
  auto Func = llvm2alive(F, TLI.getTLI(F), true);
  if (!Func) {
    cerr << "ERROR: Could not translate '" << F.getName().str()
         << "' to Alive IR\n";
    return {};
  }

  if (!config::quiet)
    Func->print(cout << "\n----------------------------------------\n");

  {
    TypingAssignments types{Func->getTypeConstraints()};
    if (!types) {
      cerr << "Internal ERROR: program doesn't type check!\n\n";
      return {};
    }
    assert(types.hasSingleTyping());
  }

  auto error = [&](const Result &r) {
    if (r.isSat() || r.isUnsat())
      return false;

    if (r.isInvalid()) {
      cerr << "ERROR: invalid expression\n";
    } else if (r.isError()) {
      cerr << "ERROR: Error in SMT solver: " << r.getReason() << '\n';
    } else if (r.isTimeout()) {
      cerr << "ERROR: SMT solver timedout\n";
    } else if (r.isSkip()) {
      cerr << "ERROR: SMT queries disabled";
    } else {
      UNREACHABLE();
    }
    return true;
  };

  try {
    State state(*Func, true);
    sym_exec_init(state);

    const BasicBlock *curr_bb = &Func->getFirstBB();

    // #init block has been executed already by sym_exec_init
    if (curr_bb->getName() == "#init") {
      curr_bb = &Func->getBB(1);
    }
    state.startBB(*curr_bb);

    auto It = curr_bb->instrs().begin();
    Solver solver(true);

    if (!config::quiet)
      cout << "Executing " << curr_bb->getName() << '\n';

    while (true) {
      auto &next_instr = *It;

      state.cleanup(next_instr);
      if (dynamic_cast<const JumpInstr*>(&next_instr))
        state.cleanupPredecessorData();

      auto &val  = state.exec(next_instr);
      auto &name = next_instr.getName();

      solver.add(val.return_domain);
      auto r = solver.check("return domain");
      if (error(r))
        return {};

      if (dynamic_cast<const Return*>(&next_instr)) {
        assert(r.isSat());
        auto ret = eval(r, state.returnVal().val);
        if (!config::quiet)
          cout << "Returned " << ret << '\n';
        return ret;
      }

      if (auto *jmp = dynamic_cast<const JumpInstr*>(&next_instr)) {
        bool jumped = false;
        for (auto &dst : jmp->targets()) {
          expr cond = state.getJumpCond(*curr_bb, dst);
          {
            SolverPush push(solver);
            solver.add(cond);
            auto r = solver.check("jump condition");
            if (error(r))
              return {};

            if (r.isSat()) {
              if (!config::quiet)
                cout << "  >> Jump to " << dst.getName() << "\n\n";
              curr_bb = &dst;
              state.startBB(dst);
              It = dst.instrs().begin();
              jumped = true;
              break;
            }
            continue;
          }
          solver.add(cond);
          if (!jumped) {
            cerr << "ERROR: All jump destinations are unreachable!\n\n";
            return {};
          }
        }
        continue;
      }

      solver.add(val.domain());
      r = solver.check("domain");
      if (error(r))
        return {};

      if (r.isUnsat()) {
        cout << name << " = UB triggered!\n\n";
        return {};
      }

      if (!config::quiet) {
        cout << name;
        if (name[0] == '%') {
          auto v = eval(r, val.val);
          if (v.non_poison.isFalse())
            cout << " = poison";
          else
            cout << " = " << v.value;
        }
        cout << '\n';
      }

      // move to the next instruction
      ++It;
    }
  } catch (const AliveException &e) {
    cout << "ERROR: " << e.msg << '\n';
    return {};
  }
  UNREACHABLE();
}

void exec(llvm::Function &F, llvm::TargetLibraryInfoWrapperPass &TLI,
          int &ret_val, bool &ret_val_poison) {
  int64_t n;
  auto ret = exec(F, TLI);
  ret_val_poison = ret.non_poison.isFalse();
  ret_val = ret.value.isInt(n) ? (int)n : -1;
}
}

unique_ptr<Cache> cache;

int main(int argc, char **argv) {
  llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);
  llvm::InitLLVM X(argc, argv);
  llvm::EnableDebugBuffering = true;
  llvm::LLVMContext Context;

  std::string Usage =
      R"EOF(Alive2 stand-alone translation validator:
version )EOF";
  Usage += alive_version;
  Usage += R"EOF(
see alive-exec --version  for LLVM version info,

This program takes an LLVM IR file as a command-line argument.
Both .bc and .ll files are supported.

If one or more functions are specified (as a comma-separated list)
using the --funcs command line option, alive-exec will attempt to
execute them using Alive2 as an interpreter.

If no functions are specified on the command line, then alive-exec
will attempt to execute the 'main' function.
If it doesn't exist, alive-exec executes every function in the bitcode file.
)EOF";

  llvm::cl::HideUnrelatedOptions(alive_cmdargs);
  llvm::cl::ParseCommandLineOptions(argc, argv, Usage);

  auto M = openInputFile(Context, opt_file);
  if (!M.get()) {
    cerr << "Could not read bitcode from '" << opt_file << "'\n";
    return -1;
  }

#define ARGS_MODULE_VAR M
#include "llvm_util/cmd_args_def.h"

  auto &DL = M.get()->getDataLayout();
  llvm::Triple targetTriple(M.get()->getTargetTriple());
  llvm::TargetLibraryInfoWrapperPass TLI(targetTriple);

  llvm_util::initializer llvm_util_init(cerr, DL);
  smt::smt_initializer smt_init;

  auto *main_fn = findFunction(*M, "main");
  int ret_val = -1;
  bool ret_val_poison = false;

  if (main_fn && func_names.empty()) {
    State::resetGlobals();
    exec(*main_fn, TLI, ret_val, ret_val_poison);
  } else {
    for (auto &F : *M) {
      if (F.isDeclaration())
        continue;
      if (!func_names.empty() && !func_names.count(F.getName().str()))
        continue;
      State::resetGlobals();
      smt_init.reset();
      exec(F, TLI, ret_val, ret_val_poison);
    }
  }

  if (ret_val_poison) {
    cerr << "ERROR: program returned poison\n";
    return -1;
  }
  return ret_val;
}
