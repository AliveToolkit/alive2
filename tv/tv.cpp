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
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/Cloning.h"
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

llvm::cl::opt<bool> batch_opts("tv-batch-opts",
  llvm::cl::desc("Batch optimizations (clang plugin only)"),
  llvm::cl::cat(alive_cmdargs));


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
std::unique_ptr<llvm::Module> MClone;
string pass_name;

void sigalarm_handler(int) {
  parallelMgr->finishChild(/*is_timeout=*/true);
  // this is a fully asynchronous exit, skip destructors and such
  _Exit(0);
}

void printDot(const Function &tgt, int n) {
  if (opt_print_dot) {
    string prefix = to_string(n);
    tgt.writeDot(prefix.c_str());
  }
}

string toString(const Function &fn) {
  stringstream ss;
  fn.print(ss);
  return std::move(ss).str();
}

static void showStats() {
  if (opt_smt_stats)
    smt::solver_print_stats(*out);
  if (opt_alias_stats)
    IR::Memory::printAliasStats(*out);
}

static void writeBitcodeAtomically(const fs::path report_filename) {
  fs::path tmp_path;
  do {
    auto newname = report_filename.stem();
    newname += "_" + get_random_str(8) + ".bc";
    tmp_path.replace_filename(newname);
  } while (fs::exists(tmp_path));

  std::error_code EC;
  llvm::raw_fd_ostream tmp_file(tmp_path.string(), EC);
  if (EC) {
    cerr << "Alive2: Couldn't open temporary bitcode file" << endl;
    exit(1);
  }
  llvm::WriteBitcodeToFile(*MClone, tmp_file);
  tmp_file.close();

  fs::path bc_filename = tmp_path;
  if (!report_filename.empty()) {
    bc_filename = report_filename;
    bc_filename.replace_extension(".bc");
    std::rename(tmp_path.c_str(), bc_filename.c_str());
  }
  *out << "Wrote bitcode to: " << bc_filename << '\n';
}

static void emitCommandLine(ostream *out) {
#ifdef __linux__
  ifstream cmd_args("/proc/self/cmdline");
  if (!cmd_args.is_open()) {
    return;
  }
  *out << "Command line:";
  std::string arg;
  while (std::getline(cmd_args, arg, '\0'))
    *out << " '" << arg << "'";
  *out << "\n";
#endif
}

struct TVLegacyPass final : public llvm::ModulePass {
  static char ID;
  bool skip_verify = false;
  bool onlyif_src_exists = false; // Verify this pair only if src exists
  const function<llvm::TargetLibraryInfo*(llvm::Function&)> *TLI_override
    = nullptr;
  unsigned anon_count;

  TVLegacyPass() : ModulePass(ID) {}

  bool runOnModule(llvm::Module &M) override {
    anon_count = 0;
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

    string name = F.getName().str();
    if (name.empty())
      name = "anon$" + std::to_string(++anon_count);
    auto [I, first] = fns.try_emplace(std::move(name));
    if (onlyif_src_exists && first) {
      // src does not exist; skip this fn
      fns.erase(I);
      return false;
    }

    auto fn = llvm2alive(F, *TLI, first,
                         first ? vector<string_view>()
                               : I->second.fn.getGlobalVarNames());
    if (!fn) {
      fns.erase(I);
      return false;
    }

    if (first || skip_verify) {
      I->second.fn = std::move(*fn);
      if (!opt_always_verify)
        // Prepare syntactic check
        I->second.fn_tostr = toString(I->second.fn);
      printDot(I->second.fn, I->second.n++);
      return false;
    }

    Transform t;
    t.src = std::move(I->second.fn);
    t.tgt = std::move(*fn);

    verify(t, I->second.n++, I->second.fn_tostr);
    I->second.fn = *llvm2alive(F, *TLI, true);
    I->second.fn_tostr = toString(I->second.fn);
    return false;
  }

  // If it returns true, the caller should regenerate tgt using llvm2alive().
  // If it returns false, the caller can simply move t.tgt to info.fn
  static bool verify(Transform &t, int n, const string &src_tostr) {
    printDot(t.tgt, n);

    if (!opt_always_verify) {
      // Compare Alive2 IR and skip if syntactically equal
      if (src_tostr == toString(t.tgt)) {
        if (!opt_quiet)
          t.print(*out, print_opts);
        *out << "Transformation seems to be correct! (syntactically equal)\n\n";
        return false;
      }
    }

    if (parallelMgr) {
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
      *out << "Transformation doesn't verify!" <<
              (errs.isUnsound() ? " (unsound)\n" : " (not unsound)\n")
           << errs;
      if (errs.isUnsound()) {
        has_failure = true;
        *out << "\nPass: " << pass_name << '\n';
        emitCommandLine(out);
        if (MClone)
          writeBitcodeAtomically(report_filename);
        *out << "\n";
      }
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
      showStats();
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
    MClone = nullptr;
    if (parallelMgr) {
      parallelMgr->finishParent();
      out = out_file.is_open() ? &out_file : &cout;
      set_outs(*out);
    }

    // If it is run in parallel, stats are shown by children
    if (!showed_stats && !parallelMgr) {
      showed_stats = true;
      showStats();
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


// List 'leaf' interprocedural passes only.
// For example, ModuleInlinerWrapperPass shouldn't be here because it is an
// interprocedural pass having other passes as children.
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
  static string batched_pass_begin_name;
  static bool batch_started;
  // # of run passes when batching is enabled
  static unsigned batched_pass_count;

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

    if (batch_opts) {
      // Batching is supported by clang-tv only
      assert(is_clangtv);

      // If set_src is true, set M as src.
      bool set_src = !batch_started;

      TVLegacyPass tv;

      if (set_src) {
        // Prepare src. Do this by setting skip_pass to true.
        tv.skip_verify = true;
        *out << "-- FROM THE BITCODE AFTER "
              << batched_pass_count << ". " << batched_pass_begin_name << "\n";
      } else {
        *out << "-- TO THE BITCODE AFTER "
              << batched_pass_count << ". " << pass_name << "\n";

        // Translate LLVM to Alive2 only if there exists src
        tv.onlyif_src_exists = true;
      }

      tv.TLI_override = &get_TLI;
      // If skip_pass is true, this updates fns map only.
      tv.runOnModule(M);

      if (!set_src)
        *out << "-- DONE: " << batched_pass_count << ". " << pass_name << "\n";
      batch_started = !batch_started;
    } else {
      bool skip_tv = do_skip(pass_name);

      static unsigned count = 0;
      count++;
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
    }
  }
};

string TVPass::batched_pass_begin_name;
bool TVPass::batch_started;
unsigned TVPass::batched_pass_count;
unsigned TVPass::num_instances = 0;
bool is_clangtv_done = false;

void runTVPass(llvm::Module &M) {
  static optional<llvm::TargetLibraryInfoImpl> TLIImpl;
  optional<llvm::TargetLibraryInfo> TLI_holder;

  auto get_TLI = [&](llvm::Function &F) {
    if (!TLIImpl)
      TLIImpl.emplace(llvm::Triple(F.getParent()->getTargetTriple()));
    return &TLI_holder.emplace(*TLIImpl, &F);
  };

  TVPass tv;
  tv.print_pass_name = true;
  tv.run(M, get_TLI);
}

struct ClangTVFinalizePass : public llvm::PassInfoMixin<ClangTVFinalizePass> {
  llvm::PreservedAnalyses run(llvm::Module &M,
                              llvm::ModuleAnalysisManager &AM) {
    if (is_clangtv) {
      if (batch_opts && TVPass::batch_started)
        runTVPass(M);

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
          [](llvm::ModulePassManager &MPM, llvm::OptimizationLevel) {
            MPM.addPass(ClangTVFinalizePass());
          });

      if (batch_opts) {
        // For batched clang tv, manually run TVPass before each pass
        PB.getPassInstrumentationCallbacks()
            ->registerBeforeNonSkippedPassCallback(
              [](llvm::StringRef P, llvm::Any IR) {
          assert(is_clangtv && "Batching is enabled for clang-tv only");
          if (is_clangtv_done)
            return;

          // Run only when it is at the boundary
          bool is_first = pass_name.empty();
          bool do_start = !TVPass::batch_started && do_skip(pass_name)
              && !do_skip(P);
          bool do_finish = TVPass::batch_started && !do_skip(pass_name)
              && do_skip(P);

          if (do_start)
            TVPass::batched_pass_begin_name = pass_name;
          else if (is_first)
            TVPass::batched_pass_begin_name = "beginning";

          if ((is_first || do_start) && opt_save_ir)
              MClone = llvm::CloneModule(*unwrapModule(IR));

          if (is_first || do_start || do_finish)
            runTVPass(*const_cast<llvm::Module *>(unwrapModule(IR)));
        });
        PB.getPassInstrumentationCallbacks()->registerAfterPassCallback([&](
            llvm::StringRef P, llvm::Any, const llvm::PreservedAnalyses &) {
          TVPass::batched_pass_count++;
          pass_name = P.str();
        });

      } else {
        // For non-batched clang tv, manually run TVPass after each pass
        if (opt_save_ir) {
          PB.getPassInstrumentationCallbacks()
            ->registerBeforeNonSkippedPassCallback(
              [](llvm::StringRef P, llvm::Any IR) {
                MClone = llvm::CloneModule(*unwrapModule(IR));
          });
        }
        PB.getPassInstrumentationCallbacks()->registerAfterPassCallback(
            [](llvm::StringRef P, llvm::Any IR,
                    const llvm::PreservedAnalyses &PA) {
          pass_name = P.str();
          if (!is_clangtv || is_clangtv_done)
            return;

          runTVPass(*const_cast<llvm::Module *>(unwrapModule(IR)));
        });
      }
    }
  };
}

}
