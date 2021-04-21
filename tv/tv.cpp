// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/memory.h"
#include "llvm_util/llvm2alive.h"
#include "llvm_util/utils.h"
#include "smt/smt.h"
#include "smt/solver.h"
#include "tools/transform.h"
#include "util/parallel.h"
#include "util/stopwatch.h"
#include "util/version.h"
#include "llvm/ADT/Any.h"
#include "llvm/ADT/Triple.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include <fstream>
#include <iostream>
#include <memory>
#include <random>
#include <signal.h>
#include <sstream>
#include <unistd.h>
#include <unordered_map>
#include <utility>

using namespace IR;
using namespace llvm_util;
using namespace tools;
using namespace util;
using namespace std;

#define LLVM_ARGS_PREFIX "tv-"
#define ARGS_SRC_TGT
#define ARGS_REFINEMENT
#include "llvm_util/cmd_args_list.h"

namespace {

llvm::cl::opt<string> parallel_tv("tv-parallel",
  llvm::cl::desc("Parallelization mode. Accepted values:"
                  " unrestricted (no throttling)"
                  ", fifo (use Alive2's job server)"
                  ", null (developer mode)"),
  llvm::cl::cat(alive_cmdargs));

llvm::cl::opt<int> max_subprocesses("max-subprocesses",
  llvm::cl::desc("Maximum children any single clang instance will have at one "
                 "time (default=128)"),
  llvm::cl::init(128), llvm::cl::cat(alive_cmdargs));

llvm::cl::opt<long> subprocess_timeout("tv-subprocess-timeout",
  llvm::cl::desc("Maximum time, in seconds, that a parallel TV call "
                 "will be allowed to execeute (default=infinite)"),
  llvm::cl::init(-1), llvm::cl::cat(alive_cmdargs));


struct FnInfo {
  Function fn;
  string fn_tostr;
  unsigned n = 0;
};

optional<smt::smt_initializer> smt_init;
optional<llvm_util::initializer> llvm_util_init;
TransformPrintOpts print_opts;
unordered_map<string, FnInfo> fns;
unsigned initialized = 0;
bool showed_stats = false;
bool has_failure = false;
// If is_clangtv is true, tv should exit with zero
bool is_clangtv = false;
unique_ptr<parallel> parallelMgr;
stringstream parent_ss;

void sigalarm_handler(int) {
  parallelMgr->finishChild(/*is_timeout=*/true);
  // this is a fully asynchronous exit, skip destructors and such
  _Exit(0);
}

static void printDot(const Function &tgt, int n) {
  if (opt_print_dot) {
    string prefix = to_string(n);
    tgt.writeDot(prefix.c_str());
  }
}

static string toString(const Function &fn) {
  stringstream ss;
  fn.print(ss);
  return ss.str();
}

struct TVLegacyPass final : public llvm::ModulePass {
  static char ID;
  bool skip_verify = false;
  const function<llvm::TargetLibraryInfo*(llvm::Function&)> *TLI_override
    = nullptr;

  TVLegacyPass() : ModulePass(ID) {}

  bool runOnModule(llvm::Module &M) override {
    for (auto &F: M)
      runOnFunction(F);
    return false;
  }

  bool runOnFunction(llvm::Function &F) {
    if (F.isDeclaration())
      // This can happen at EntryExitInstrumenter pass.
      return false;

    if (!func_names.empty() && !func_names.count(F.getName().str()))
      return false;

    optional<ScopedWatch> timer;
    if (opt_elapsed_time)
      timer.emplace([&](const StopWatch &sw) {
        *out << "Took " << sw.seconds() << "s\n";
      });

    llvm::TargetLibraryInfo *TLI = nullptr;
    if (TLI_override) {
      // When used as a clang plugin or from the new pass manager, this is run
      // as a plain function rather than a registered pass, so getAnalysis()
      // cannot be used.
      TLI = (*TLI_override)(F);
    } else {
      TLI = &getAnalysis<llvm::TargetLibraryInfoWrapperPass>().getTLI(F);
    }

    auto [I, first] = fns.try_emplace(F.getName().str());

    auto fn = llvm2alive(F, *TLI, first ? vector<string_view>()
                                        : I->second.fn.getGlobalVarNames());
    if (!fn) {
      fns.erase(I);
      return false;
    }

    if (first || skip_verify) {
      I->second.fn = move(*fn);
      if (!opt_always_verify)
        // Prepare syntactic check
        I->second.fn_tostr = toString(I->second.fn);
      printDot(I->second.fn, I->second.n++);
      return false;
    }

    Transform t;
    t.src = move(I->second.fn);
    t.tgt = move(*fn);

    bool regenerate_tgt = verify(t, I->second);

    if (regenerate_tgt)
      I->second.fn = *llvm2alive(F, *TLI);
    else
      I->second.fn = move(t.tgt);

    I->second.fn_tostr = toString(I->second.fn);
    I->second.n++;

    return false;
  }

  // If it returns true, the caller should regenerate tgt using llvm2alive().
  // If it returns false, the caller can simply move t.tgt to info.fn
  static bool verify(Transform &t, const FnInfo &info) {
    printDot(t.tgt, info.n);

    if (!opt_always_verify) {
      // Compare Alive2 IR and skip if syntactically equal
      if (info.fn_tostr == toString(t.tgt)) {
        if (!opt_quiet)
          t.print(*out, print_opts);
        *out << "Transformation seems to be correct! (syntactically equal)\n\n";
        return false;
      }
    }

    if (parallelMgr) {
      out_file.flush();
      auto [pid, osp, index] = parallelMgr->limitedFork();

      if (pid == -1) {
        perror("fork() failed");
        exit(-1);
      }

      if (pid != 0) {
        /*
         * parent returns to LLVM immediately; leave a placeholder in
         * the output that we'll patch up later
         */
        *out << "include(" << index << ")\n";
        /*
         * Tell the caller that tgt should be regenerated via llvm2alive.
         * TODO: this llvm2alive() call isn't needed for correctness,
         * but only to make parallel output match sequential
         * output. we can remove it later if we want.
         */
        return true;
      }

      if (subprocess_timeout != -1) {
        ENSURE(signal(SIGALRM, sigalarm_handler) == nullptr);
        alarm(subprocess_timeout);
      }

      /*
       * child now writes to a stringstream provided by the parallel
       * manager, its output will get pushed to the parent via a pipe
       * later on
       */
      out = osp;
      set_outs(*out);
    }

    /*
     * from here, we must not return back to LLVM if parallelMgr
     * is non-null; instead we call parallelMgr->finishChild()
     */

    smt_init->reset();
    t.preprocess();
    TransformVerify verifier(t, false);
    if (!opt_quiet)
      t.print(*out, print_opts);

    {
      auto types = verifier.getTypings();
      if (!types) {
        *out << "Transformation doesn't verify!\n"
                "ERROR: program doesn't type check!\n\n";
        goto done;
      }
      assert(types.hasSingleTyping());
    }

    if (Errors errs = verifier.verify()) {
      *out << "Transformation doesn't verify!\n" << errs << endl;
      has_failure |= errs.isUnsound();
      if (opt_error_fatal && has_failure)
        finalize();
    } else {
      *out << "Transformation seems to be correct!\n\n";
    }

    // Regenerate tgt because preprocessing may have changed it
    if (!parallelMgr)
      return true;

  done:
    if (parallelMgr) {
      signal(SIGALRM, SIG_IGN);
      llvm_util_init.reset();
      smt_init.reset();
      parallelMgr->finishChild(/*is_timeout=*/false);
      exit(0);
    }
    return false;
  }

  bool doInitialization(llvm::Module &module) override {
    initialize(module);
    return false;
  }

  static void initialize(llvm::Module &module) {
    if (initialized++)
      return;

#define ARGS_MODULE_VAR (&module)
#   include "llvm_util/cmd_args_def.h"

    if (parallel_tv == "unrestricted") {
      parallelMgr = make_unique<unrestricted>(max_subprocesses, parent_ss,
                                              *out);
    } else if (parallel_tv == "fifo") {
      parallelMgr = make_unique<fifo>(max_subprocesses, parent_ss, *out);
    } else if (parallel_tv == "null") {
      parallelMgr = make_unique<null>(max_subprocesses, parent_ss, *out);
    } else if (!parallel_tv.empty()) {
      *out << "Alive2: Unknown parallelization mode: " << parallel_tv << endl;
      exit(1);
    }

    if (parallelMgr) {
      if (parallelMgr->init()) {
        out = &parent_ss;
        set_outs(*out);
      } else {
        *out << "WARNING: Parallel execution of Alive2 Clang plugin is "
                "unavailable, sorry\n";
        parallelMgr.reset();
      }
    }

    showed_stats = false;
    llvm_util_init.emplace(*out, module.getDataLayout());
    smt_init.emplace();
    return;
  }

  bool doFinalization(llvm::Module&) override {
    finalize();
    return false;
  }

  static void finalize() {
    if (parallelMgr) {
      parallelMgr->finishParent();
      out = out_file.is_open() ? &out_file : &cout;
      set_outs(*out);
    }

    if (!showed_stats) {
      showed_stats = true;
      if (opt_smt_stats)
        smt::solver_print_stats(*out);
      if (opt_alias_stats)
        IR::Memory::printAliasStats(*out);
      if (has_failure && !report_filename.empty())
        cerr << "Report written to " << report_filename << endl;
    }

    llvm_util_init.reset();
    smt_init.reset();
    --initialized;

    if (has_failure) {
      if (opt_error_fatal)
        *out << "Alive2: Transform doesn't verify; aborting!" << endl;
      else
        *out << "Alive2: Transform doesn't verify!" << endl;

      if (!is_clangtv)
        exit(1);
    }
  }

  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override {
    AU.addRequired<llvm::TargetLibraryInfoWrapperPass>();
    AU.setPreservesAll();
  }
};

char TVLegacyPass::ID = 0;
llvm::RegisterPass<TVLegacyPass> X("tv", "Translation Validator", false, false);



/// Classes and functions for running translation validation on clang or
/// opt with new pass manager
/// Clang plugin uses new pass manager's callback.

// Extracting Module out of IR unit.
// Excerpted from LLVM's StandardInstrumentation.cpp
const llvm::Module * unwrapModule(llvm::Any IR) {
  using namespace llvm;

  if (any_isa<const Module *>(IR))
    return any_cast<const Module *>(IR);
  else if (any_isa<const llvm::Function *>(IR))
    return any_cast<const llvm::Function *>(IR)->getParent();
  else if (any_isa<const LazyCallGraph::SCC *>(IR)) {
    auto C = any_cast<const LazyCallGraph::SCC *>(IR);
    assert(C->begin() != C->end()); // there's at least one function
    return C->begin()->getFunction().getParent();
  } else if (any_isa<const Loop *>(IR))
    return any_cast<const Loop *>(IR)->getHeader()->getParent()->getParent();

  llvm_unreachable("Unknown IR unit");
}


const char* skip_pass_list[] = {
  "ArgumentPromotionPass",
  "DeadArgumentEliminationPass",
  "EliminateAvailableExternallyPass",
  "EntryExitInstrumenterPass",
  "GlobalOptPass",
  "HotColdSplittingPass",
  "InferFunctionAttrsPass", // IPO
  "InlinerPass",
  "IPSCCPPass",
  "ModuleInlinerWrapperPass",
  "OpenMPOptPass",
  "PostOrderFunctionAttrsPass", // IPO
  "TailCallElimPass",
};

bool do_skip(const llvm::StringRef &pass0) {
  auto pass = pass0.str();
  return any_of(skip_pass_list, end(skip_pass_list),
                [&](auto skip) { return pass == skip; });
}

struct TVPass : public llvm::PassInfoMixin<TVPass> {
  static bool skip_tv;
  static string pass_name;
  bool print_pass_name = false;
  // A reference counter for TVPass objects.
  // If this counter reaches zero, finalization should be called.
  // Note that this is necessary for opt + NPM only.
  // (1) In case of opt + LegacyPM, we can use TVLegacyPass::doFinalization().
  // (2) In case of clang tv, we have registerOptimizerLastEPCallback.
  static unsigned num_instances;
  bool is_moved = false;

  TVPass() { ++num_instances; }
  TVPass(TVPass &&other) {
    other.is_moved = true;
    print_pass_name = other.print_pass_name;
  }
  ~TVPass() {
    assert(num_instances >= (unsigned)!is_moved);
    num_instances -= !is_moved;
    if (initialized && num_instances == 0 && !is_clangtv) {
      // All TVPass instances are deleted.
      // This happens when llvm::runPassPipeline is done.
      // If it isn't clang tv (which has ClangTVFinalizePass to control
      // finalization), finalize resources.
      TVLegacyPass::finalize();
    }
  }
  TVPass(const TVPass &) = delete;
  TVPass &operator=(const TVPass &) = delete;

  llvm::PreservedAnalyses run(llvm::Module &M,
                              llvm::ModuleAnalysisManager &AM) {
    auto &FAM = AM.getResult<llvm::FunctionAnalysisManagerModuleProxy>(M)
                  .getManager();
    auto get_TLI = [&FAM](llvm::Function &F) {
      return &FAM.getResult<llvm::TargetLibraryAnalysis>(F);
    };
    run(M, get_TLI);
    return llvm::PreservedAnalyses::all();
  }

  void run(llvm::Module &M,
           const function<llvm::TargetLibraryInfo*(llvm::Function&)> &get_TLI) {
    if (!initialized)
      TVLegacyPass::initialize(M);

    static unsigned count = 0;

    ++count;
    if (print_pass_name) {
      // print_pass_name is set only when running clang tv
      *out << "-- " << count << ". " << pass_name
           << (skip_tv ? " : Skipping\n" : "\n");
    }

    TVLegacyPass tv;
    tv.skip_verify = skip_tv;

    tv.TLI_override = &get_TLI;
    // If skip_pass is true, this updates fns map only.
    tv.runOnModule(M);

    skip_tv = false;
  }
};

bool TVPass::skip_tv = false;
string TVPass::pass_name;
unsigned TVPass::num_instances = 0;
bool is_clangtv_done = false;

struct ClangTVFinalizePass : public llvm::PassInfoMixin<ClangTVFinalizePass> {
  llvm::PreservedAnalyses run(llvm::Module &M,
                              llvm::ModuleAnalysisManager &AM) {
    if (is_clangtv) {
      if (initialized)
        TVLegacyPass::finalize();
      is_clangtv_done = true;
    }
    return llvm::PreservedAnalyses::all();
  }
};

// Entry point for this plugin
extern "C" ::llvm::PassPluginLibraryInfo LLVM_ATTRIBUTE_WEAK
llvmGetPassPluginInfo() {
  return {
    LLVM_PLUGIN_API_VERSION, "Alive2 Translation Validation", "",
    [](llvm::PassBuilder &PB) {
      is_clangtv = true;
      PB.registerPipelineParsingCallback(
          [](llvm::StringRef Name,
             llvm::ModulePassManager &MPM,
             llvm::ArrayRef<llvm::PassBuilder::PipelineElement>) {
          if (Name != "tv")
            return false;

          // Assume that this plugin is loaded from opt when tv pass is
          // explicitly given as an argument
          is_clangtv = false;

          MPM.addPass(TVPass());
          return true;
        });
      // registerOptimizerLastEPCallback is called when 'default' pipelines
      // such as O2, O3 are used by either opt or clang.
      // ClangTVFinalizePass internally checks whether we're running clang tv
      // and finalizes resources then.
      PB.registerOptimizerLastEPCallback(
          [](llvm::ModulePassManager &MPM,
             llvm::PassBuilder::OptimizationLevel) {
            MPM.addPass(ClangTVFinalizePass());
          });
      auto clang_tv = [](llvm::StringRef P, llvm::Any IR,
                  const llvm::PreservedAnalyses &PA) {
        TVPass::pass_name = P.str();
        TVPass::skip_tv |= do_skip(TVPass::pass_name);
        if (!is_clangtv)
          return;
        else if (is_clangtv_done)
          return;

        // If it is clang tv, validate each pass
        TVPass tv;
        tv.print_pass_name = true;

        static optional<llvm::TargetLibraryInfoImpl> TLIImpl;
        optional<llvm::TargetLibraryInfo> TLI_holder;

        auto get_TLI = [&](llvm::Function &F) {
          if (!TLIImpl)
            TLIImpl.emplace(llvm::Triple(F.getParent()->getTargetTriple()));
          return &TLI_holder.emplace(*TLIImpl, &F);
        };

        tv.run(*const_cast<llvm::Module *>(unwrapModule(IR)), get_TLI);
      };
      // For clang tv, manually run TVPass after each pass
      PB.getPassInstrumentationCallbacks()->registerAfterPassCallback(
          move(clang_tv));
    }
  };
}

}
