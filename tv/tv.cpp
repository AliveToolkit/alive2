// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/memory.h"
#include "llvm_util/llvm2alive.h"
#include "llvm_util/utils.h"
#include "smt/smt.h"
#include "smt/solver.h"
#include "tools/transform.h"
#include "util/config.h"
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
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"
#include <filesystem>
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
namespace fs = std::filesystem;

namespace {

llvm::cl::OptionCategory TVOptions("Alive translation verifier options");

llvm::cl::opt<bool> opt_error_fatal("tv-exit-on-error",
                                    llvm::cl::desc("Alive: exit on error"),
                                    llvm::cl::cat(TVOptions),
                                    llvm::cl::init(false));

llvm::cl::opt<unsigned>
    opt_smt_to("tv-smt-to", llvm::cl::desc("Alive: timeout for SMT queries"),
               llvm::cl::cat(TVOptions), llvm::cl::init(1000),
               llvm::cl::value_desc("ms"));

llvm::cl::opt<unsigned> opt_smt_random_seed(
    "tv-smt-random-seed",
    llvm::cl::desc("Alive: Random seed for the SMT solver (default=0)"),
    llvm::cl::cat(TVOptions), llvm::cl::init(0));

llvm::cl::opt<unsigned> opt_max_mem("tv-max-mem",
                                    llvm::cl::desc("Alive: max memory (aprox)"),
                                    llvm::cl::cat(TVOptions),
                                    llvm::cl::init(1024),
                                    llvm::cl::value_desc("MB"));

llvm::cl::opt<bool>
    opt_se_verbose("tv-se-verbose",
                   llvm::cl::desc("Alive: symbolic execution verbose mode"),
                   llvm::cl::cat(TVOptions), llvm::cl::init(false));

llvm::cl::opt<bool> opt_smt_stats("tv-smt-stats",
                                  llvm::cl::desc("Alive: show SMT statistics"),
                                  llvm::cl::cat(TVOptions),
                                  llvm::cl::init(false));

llvm::cl::opt<bool>
    opt_succinct("tv-succinct",
                 llvm::cl::desc("Alive2: make the output succinct"),
                 llvm::cl::cat(TVOptions), llvm::cl::init(false));

llvm::cl::opt<bool>
    opt_alias_stats("tv-alias-stats",
                    llvm::cl::desc("Alive: show alias sets statistics"),
                    llvm::cl::cat(TVOptions), llvm::cl::init(false));

llvm::cl::opt<bool> opt_smt_skip("tv-smt-skip",
                                 llvm::cl::desc("Alive: skip SMT queries"),
                                 llvm::cl::cat(TVOptions),
                                 llvm::cl::init(false));

llvm::cl::opt<string>
    opt_report_dir("tv-report-dir",
                   llvm::cl::desc("Alive: save report to disk"),
                   llvm::cl::cat(TVOptions), llvm::cl::value_desc("directory"));

llvm::cl::opt<bool> opt_overwrite_reports(
    "tv-overwrite-reports",
    llvm::cl::desc("Alive: overwrite existing report files"),
    llvm::cl::cat(TVOptions), llvm::cl::init(false));

llvm::cl::opt<bool> opt_smt_verbose("tv-smt-verbose",
                                    llvm::cl::desc("Alive: SMT verbose mode"),
                                    llvm::cl::cat(TVOptions),
                                    llvm::cl::init(false));

llvm::cl::opt<bool>
    opt_tactic_verbose("tv-tactic-verbose",
                       llvm::cl::desc("Alive: SMT Tactic verbose mode"),
                       llvm::cl::cat(TVOptions), llvm::cl::init(false));

llvm::cl::opt<bool>
    opt_smt_log("tv-smt-log",
                llvm::cl::desc("Alive: log interactions with the SMT solver"),
                llvm::cl::cat(TVOptions), llvm::cl::init(false));

llvm::cl::opt<string>
    opt_smt_bench_dir("tv-smt-bench",
                      llvm::cl::desc("Alive: dump smtlib benchmarks"),
                      llvm::cl::cat(TVOptions),
                      llvm::cl::value_desc("directory"));

llvm::cl::opt<bool> opt_print_dot(
    "tv-dot",
    llvm::cl::desc("Alive: print .dot files of each function"),
    llvm::cl::cat(TVOptions), llvm::cl::init(false));

llvm::cl::opt<bool> opt_disable_poison_input(
    "tv-disable-poison-input",
    llvm::cl::desc("Alive: Assume function input cannot be poison"),
    llvm::cl::cat(TVOptions), llvm::cl::init(false));

llvm::cl::opt<bool> opt_disable_undef_input(
    "tv-disable-undef-input",
    llvm::cl::desc("Alive: Assume function input cannot be undef"),
    llvm::cl::cat(TVOptions), llvm::cl::init(false));

llvm::cl::list<string>
    opt_funcs("tv-func",
              llvm::cl::desc("Name of functions to verify (without @)"),
              llvm::cl::cat(TVOptions), llvm::cl::ZeroOrMore,
              llvm::cl::value_desc("function name"));

llvm::cl::opt<bool> opt_debug("tv-dbg",
                              llvm::cl::desc("Alive: Show debug data"),
                              llvm::cl::cat(TVOptions), llvm::cl::init(false),
                              llvm::cl::Hidden);

llvm::cl::opt<unsigned> opt_omit_array_size(
    "tv-omit-array-size",
    llvm::cl::desc("Omit an array initializer if it has elements more than "
                   "this number"),
    llvm::cl::cat(TVOptions), llvm::cl::init(-1));

llvm::cl::opt<bool> opt_elapsed_time(
    "tv-elapsed-time",
    llvm::cl::desc("Print the elapsed time"),
    llvm::cl::cat(TVOptions), llvm::cl::init(false));

llvm::cl::opt<unsigned> opt_src_unrolling_factor(
    "tv-src-unroll",
    llvm::cl::desc("Unrolling factor for src function (default=0)"),
    llvm::cl::cat(TVOptions), llvm::cl::init(0));

llvm::cl::opt<unsigned> opt_tgt_unrolling_factor(
    "tv-tgt-unroll",
    llvm::cl::desc("Unrolling factor for tgt function (default=0)"),
    llvm::cl::cat(TVOptions), llvm::cl::init(0));

llvm::cl::opt<size_t> opt_max_offset_in_bits(
    "tv-max-offset-in-bits", llvm::cl::init(64), llvm::cl::cat(TVOptions),
    llvm::cl::desc("Upper bound for the maximum pointer offset in bits.   Note "
                   "that this may impact correctness, if values involved in "
                   "offset computations exceed the maximum."));

llvm::cl::opt<bool> parallel_tv_unrestricted(
    "tv-parallel-unrestricted",
    llvm::cl::desc("Distribute TV load across cores without any throttling; "
                   "use this very carefully, if at all (default=false)"),
    llvm::cl::cat(TVOptions), llvm::cl::init(false));

llvm::cl::opt<bool> parallel_tv_fifo(
    "tv-parallel-fifo",
    llvm::cl::desc("Distribute TV load across cores using Alive2's job "
                   "server (please see README.md, default=false)"),
    llvm::cl::cat(TVOptions), llvm::cl::init(false));

llvm::cl::opt<bool> parallel_tv_null(
    "tv-parallel-null",
    llvm::cl::desc("Pretend to fork off child processes but don't really "
                   "do it. For developer use only (default=false)"),
    llvm::cl::cat(TVOptions), llvm::cl::init(false));

llvm::cl::opt<int> max_subprocesses(
    "tv-max-subprocesses",
    llvm::cl::desc("Maximum children any single clang instance will have at one "
                   "time (default=128)"),
    llvm::cl::cat(TVOptions), llvm::cl::init(128));

llvm::cl::opt<long> subprocess_timeout(
    "tv-subprocess-timeout",
    llvm::cl::desc("Maximum time, in seconds, that a parallel TV call "
                   "will be allowed to execeute (default=infinite)"),
    llvm::cl::cat(TVOptions), llvm::cl::init(-1));

struct FnInfo {
  Function fn;
  unsigned order;
  string fn_tostr;
};

ostream *out;
ofstream out_file;
string report_filename;
optional<smt::smt_initializer> smt_init;
optional<llvm_util::initializer> llvm_util_init;
TransformPrintOpts print_opts;
unordered_map<string, FnInfo> fns;
unordered_map<string, float> fns_elapsed_time;
set<string> fnsToVerify;
unsigned initialized = 0;
bool showed_stats = false;
bool report_dir_created = false;
bool has_failure = false;
// If is_clangtv is true, tv should exit with zero
bool is_clangtv = false;
fs::path opt_report_parallel_dir;
unique_ptr<parallel> parallelMgr;
stringstream parent_ss;

void sigalarm_handler(int) {
  parallelMgr->finishChild(/*is_timeout=*/true);
  // this is a fully asynchronous exit, skip destructors and such
  _Exit(0);
}

string get_random_str() {
  static default_random_engine re;
  static uniform_int_distribution<unsigned> rand;
  static bool seeded = false;

  if (!seeded) {
    random_device rd;
    re.seed(rd());
    seeded = true;
  }
  return to_string(rand(re));
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

    if (!fnsToVerify.empty() && !fnsToVerify.count(F.getName().str()))
      return false;

    optional<ScopedWatch> timer;
    if (opt_elapsed_time)
      timer.emplace([&](const StopWatch &sw) {
        fns_elapsed_time[F.getName().str()] += sw.seconds();
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

    if (is_clangtv) {
      // Compare Alive2 IR and skip if syntactically equal
      stringstream ss;
      fn->print(ss);

      string str2 = ss.str();
      // Optimization: since string comparison can be expensive for big
      // functions, skip it if skip_verify is true.
      // verifier.verify() will never happen if skip_verify is true, so
      // there is nothing to prune early.
      if (!skip_verify && I->second.fn_tostr == str2)
        return false;

      I->second.fn_tostr = move(str2);
    }

    auto old_fn = move(I->second.fn);
    I->second.fn = move(*fn);

    if (opt_print_dot) {
      string prefix = to_string(I->second.order);
      I->second.fn.writeDot(prefix.c_str());
    }

    if (first || skip_verify)
      return false;

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
         * TODO: this llvm2alive() call isn't needed for correctness,
         * but only to make parallel output match sequential
         * output. we can remove it later if we want.
         */
        I->second.fn = *llvm2alive(F, *TLI);
        return false;
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
    Transform t;
    t.src = move(old_fn);
    t.tgt = move(I->second.fn);
    t.preprocess();
    TransformVerify verifier(t, false);
    if (!opt_succinct)
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
        doFinalization(*F.getParent());
    } else {
      *out << "Transformation seems to be correct!\n\n";
    }

    // Regenerate tgt because preprocessing may have changed it
    if (!parallelMgr)
      I->second.fn = *llvm2alive(F, *TLI);

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

    fnsToVerify.insert(opt_funcs.begin(), opt_funcs.end());

    if (!report_dir_created && !opt_report_dir.empty()) {
      try {
        fs::create_directories(opt_report_dir.getValue());
      } catch (...) {
        cerr << "Alive2: Couldn't create report directory!" << endl;
        exit(1);
      }
      auto &source_file = module.getSourceFileName();
      fs::path fname = source_file.empty() ? "alive.txt" : source_file;
      fname.replace_extension(".txt");
      fs::path path = fs::path(opt_report_dir.getValue()) / fname.filename();

      if (!opt_overwrite_reports) {
        // NB there's a low-probability toctou race here
        do {
          auto newname = fname.stem();
          newname += "_" + get_random_str() + ".txt";
          path.replace_filename(newname);
        } while (fs::exists(path));
      }

      out_file = ofstream(path);
      out = &out_file;
      if (!out_file.is_open()) {
        cerr << "Alive2: Couldn't open report file!" << endl;
        exit(1);
      }

      report_filename = path;
      *out << "Source: " << source_file << endl;
      report_dir_created = true;

      if (opt_smt_log) {
        fs::path path_z3log = path;
        path_z3log.replace_extension("z3_log.txt");
        smt::start_logging(path_z3log.c_str());
      }
    } else if (opt_report_dir.empty()) {
      out = &cerr;
      if (opt_smt_log) {
        smt::start_logging();
      }
    }

    auto &outstream = out_file.is_open() ? out_file : cerr;
    if (parallel_tv_unrestricted) {
      parallelMgr = make_unique<unrestricted>(max_subprocesses, parent_ss,
                                              outstream);
    }
    if (parallel_tv_fifo) {
      if (parallelMgr) {
        cerr << "Alive2: Please specify only one parallel manager" << endl;
        exit(1);
      }
      parallelMgr = make_unique<fifo>(max_subprocesses, parent_ss,
                                      outstream);
    }
    if (parallel_tv_null) {
      if (parallelMgr) {
        cerr << "Alive2: Please specify only one parallel manager" << endl;
        exit(1);
      }
      parallelMgr = make_unique<null>(max_subprocesses, parent_ss,
                                      outstream);
    }

    if (parallelMgr) {
      if (parallelMgr->init()) {
        out = &parent_ss;
        set_outs(*out);
      } else {
        cerr << "WARNING: Parallel execution of Alive2 Clang plugin is "
                "unavailable, sorry\n";
        parallelMgr.reset();
      }
    }

    showed_stats = false;
    smt::solver_print_queries(opt_smt_verbose);
    smt::solver_tactic_verbose(opt_tactic_verbose);
    smt::set_query_timeout(to_string(opt_smt_to));
    smt::set_random_seed(to_string(opt_smt_random_seed));
    smt::set_memory_limit(opt_max_mem * 1024 * 1024);
    config::skip_smt = opt_smt_skip;
    config::smt_benchmark_dir = opt_smt_bench_dir;

    config::symexec_print_each_value = opt_se_verbose;
    config::disable_undef_input = opt_disable_undef_input;
    config::disable_poison_input = opt_disable_poison_input;
    config::debug = opt_debug;
    config::src_unroll_cnt = opt_src_unrolling_factor;
    config::tgt_unroll_cnt = opt_tgt_unrolling_factor;
    config::max_offset_bits = opt_max_offset_in_bits;
    llvm_util::omit_array_size = opt_omit_array_size;

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
      out = out_file.is_open() ? &out_file : &cerr;
      set_outs(*out);
    }

    if (!showed_stats) {
      showed_stats = true;
      if (opt_smt_stats)
        smt::solver_print_stats(*out);
      if (opt_alias_stats)
        IR::Memory::printAliasStats(cout);
      if (has_failure && !report_filename.empty())
        cerr << "Report written to " << report_filename << endl;
    }

    llvm_util_init.reset();
    smt_init.reset();
    --initialized;

    if (opt_elapsed_time) {
      *out << "\n----------------- ELAPSED TIME ------------------\n";
      float total = 0;
      for (auto &[name, t]: fns_elapsed_time) {
        *out << "  " << name << ": " << t << " s\n";
        total += t;
      }
      *out << "  <TOTAL>: " << total << " s\n";
    }

    if (has_failure) {
      if (opt_error_fatal)
        cerr << "Alive2: Transform doesn't verify; aborting!" << endl;
      else
        cerr << "Alive2: Transform doesn't verify!" << endl;

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
