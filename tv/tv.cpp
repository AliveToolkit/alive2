// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "llvm_util/llvm2alive.h"
#include "ir/memory.h"
#include "smt/smt.h"
#include "smt/solver.h"
#include "tools/transform.h"
#include "util/config.h"
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
#include <random>
#include <sstream>
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

llvm::cl::list<std::string>
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

llvm::cl::opt<bool> opt_io_nobuiltin(
    "tv-io-nobuiltin",
    llvm::cl::desc("Encode standard I/O functions as an unknown function "
                   "(unused by clang plugin)"),
    llvm::cl::cat(TVOptions), llvm::cl::init(false));

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

struct FnInfo {
  Function fn;
  unsigned order;
  std::string fn_tostr;
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
bool is_clangtv = false;


struct TVPass final : public llvm::FunctionPass {
  static char ID;
  bool skip_verify = false;
  bool encode_io_fns_as_unknown = false;

  TVPass() : FunctionPass(ID) {}

  bool runOnFunction(llvm::Function &F) override {
    if (F.isDeclaration())
      // This can happen at EntryExitInstrumenter pass.
      return false;

    if (!fnsToVerify.empty() && !fnsToVerify.count(F.getName().str()))
      return false;

    ScopedWatch timer([&] (const StopWatch &sw) {
      fns_elapsed_time[F.getName().str()] += sw.seconds();
    });

    llvm::TargetLibraryInfo *TLI = nullptr;
    llvm::TargetLibraryInfoImpl TLIImpl;
    unique_ptr<llvm::TargetLibraryInfo> TLI_holder;
    if (is_clangtv) {
      // When used as a clang plugin, this is run as a plain function rather
      // than a registered pass, so getAnalysis() cannot be used.
      TLIImpl = llvm::TargetLibraryInfoImpl(
          llvm::Triple(F.getParent()->getTargetTriple()));
      TLI_holder = make_unique<llvm::TargetLibraryInfo>(TLIImpl, &F);
      TLI = TLI_holder.get();
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

    if (is_clangtv)
      I->second.fn.setFnCallValidFlag(encode_io_fns_as_unknown);

    smt_init->reset();
    Transform t;
    t.src = move(old_fn);
    t.tgt = move(I->second.fn);
    t.preprocess();
    TransformVerify verifier(t, false);
    if (!opt_succinct)
      t.print(*out, print_opts);

    if (is_clangtv)
      I->second.fn.setFnCallValidFlag(false);

    {
      auto types = verifier.getTypings();
      if (!types) {
        *out << "Transformation doesn't verify!\n"
                "ERROR: program doesn't type check!\n\n";
        return false;
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
    I->second.fn = *llvm2alive(F, *TLI);
    return false;
  }

  bool doInitialization(llvm::Module &module) override {
    if (initialized++)
      return false;

    fnsToVerify.insert(opt_funcs.begin(), opt_funcs.end());

    if (!report_dir_created && !opt_report_dir.empty()) {
      static default_random_engine re;
      static uniform_int_distribution<unsigned> rand;
      static bool seeded = false;

      if (!seeded) {
        random_device rd;
        re.seed(rd());
        seeded = true;
      }

      fs::create_directories(opt_report_dir.getValue());
      auto &source_file = module.getSourceFileName();
      fs::path fname = source_file.empty() ? "alive.txt" : source_file;
      fname.replace_extension(".txt");
      fs::path path = fs::path(opt_report_dir.getValue()) / fname.filename();

      if (!opt_overwrite_reports) {
        do {
          auto newname = fname.stem();
          newname += "_" + to_string(rand(re)) + ".txt";
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

    showed_stats = false;
    smt::solver_print_queries(opt_smt_verbose);
    smt::solver_tactic_verbose(opt_tactic_verbose);
    smt::set_query_timeout(to_string(opt_smt_to));
    smt::set_random_seed(to_string(opt_smt_random_seed));
    smt::set_memory_limit(opt_max_mem * 1024 * 1024);
    config::skip_smt = opt_smt_skip;

    if (!is_clangtv)
      config::io_nobuiltin = opt_io_nobuiltin;
    else {
      config::io_nobuiltin = true;
      if (opt_io_nobuiltin)
        cerr << "Warning: -tv-io-nobuiltin isn't used by clang plugin. I/O"
                " function calls will be always regarded as unknown fn calls"
                " except InstCombine.\n";
    }

    config::symexec_print_each_value = opt_se_verbose;
    config::disable_undef_input = opt_disable_undef_input;
    config::disable_poison_input = opt_disable_poison_input;
    config::debug = opt_debug;
    config::src_unroll_cnt = opt_src_unrolling_factor;
    config::tgt_unroll_cnt = opt_tgt_unrolling_factor;
    llvm_util::omit_array_size = opt_omit_array_size;

    llvm_util_init.emplace(*out, module.getDataLayout());
    smt_init.emplace();
    return false;
  }

  bool doFinalization(llvm::Module&) override {
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
    return false;
  }

  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override {
    AU.addRequired<llvm::TargetLibraryInfoWrapperPass>();
    AU.setPreservesAll();
  }
};

char TVPass::ID = 0;
llvm::RegisterPass<TVPass> X("tv", "Translation Validator", false, false);



#ifdef CLANG_PLUGIN
/// Classes and functions for running translation validation on clang
/// Uses new pass manager's callback.

struct TVInitPass : public llvm::PassInfoMixin<TVInitPass> {
  llvm::PreservedAnalyses run(llvm::Module &M,
                              llvm::ModuleAnalysisManager &MAM) {
    TVPass().doInitialization(M);
    return llvm::PreservedAnalyses::all();
  }
};

struct TVFinalizePass : public llvm::PassInfoMixin<TVFinalizePass> {
  static bool finalized;
  llvm::PreservedAnalyses run(llvm::Function &F,
                              llvm::FunctionAnalysisManager &FAM) {
    if (!finalized) {
      finalized = true;
      TVPass().doFinalization(*F.getParent());
    }
    return llvm::PreservedAnalyses::all();
  }
};

bool TVFinalizePass::finalized = false;

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

bool do_skip(const llvm::StringRef &ref) {
  const vector<string_view> pass_list = {
    "::TVInitPass", "::TVFinalizePass",
    "ArgumentPromotionPass", "DeadArgumentEliminationPass",
    "HotColdSplittingPass", "InlinerPass",
    "GlobalOptPass", "IPSCCPPass",
    "ModuleInlinerWrapperPass", // inliner pass wrapper
    "OpenMPOptPass", // open mp optimization (concurrency)
    "PostOrderFunctionAttrsPass", // changes fn signatures
    "InferFunctionAttrsPass", // changes fn signatures
    "EntryExitInstrumenterPass", // instruments profiler-related fn calls
    "EliminateAvailableExternallyPass", // Del. available_externally linkage fns
  };
  auto sref = ref.str();
  auto ends_with = [](const string_view &a, const string_view &suffix) {
    return a.size() >= suffix.size() &&
           a.substr(a.size() - suffix.size()) == suffix;
  };
  return
    std::find_if(pass_list.begin(), pass_list.end(), [&](string_view elem) {
        return ends_with(sref, elem);
      }) != pass_list.end();
}

// Entry point for this plugin
extern "C" ::llvm::PassPluginLibraryInfo LLVM_ATTRIBUTE_WEAK
llvmGetPassPluginInfo() {
  return {
    LLVM_PLUGIN_API_VERSION, "Alive2 Translation Validation", "",
    [](llvm::PassBuilder &PB) {
      is_clangtv = true;
      PB.registerPipelineStartEPCallback(
          [](llvm::ModulePassManager &MPM,
             llvm::PassBuilder::OptimizationLevel) {
            MPM.addPass(TVInitPass());
          });
      PB.registerOptimizerLastEPCallback(
          [](llvm::ModulePassManager &MPM,
             llvm::PassBuilder::OptimizationLevel) {
            MPM.addPass(createModuleToFunctionPassAdaptor(TVFinalizePass()));
          });
      auto f = [](llvm::StringRef P, llvm::Any IR,
                  const llvm::PreservedAnalyses &PA) {
        static int count = 0;
        if (!out) {
          // TVInitPass is not called yet.
          // This can happen at very early passes, such as
          // ForceFunctionAttrsPass.
          return;
        }

        if (TVFinalizePass::finalized)
          return;

        bool skip_pass = do_skip(P);
        *out << "-- " << ++count << ". " << P.str()
             << (skip_pass ? " : Skipping\n" : "\n");

        TVPass tv;
        tv.skip_verify = skip_pass;
        // For I/O known calls like printf, it is fine to regard them as valid
        // 'unknown calls' except when it is InstCombine.
        tv.encode_io_fns_as_unknown = P != "InstCombinePass" &&
                                      P != "AggressiveInstCombinePass";

        auto M = const_cast<llvm::Module *>(unwrapModule(IR));
        for (auto &F: *M)
          // If skip_pass is true, this updates fns map only.
          tv.runOnFunction(F);
      };
      PB.getPassInstrumentationCallbacks()->registerAfterPassCallback(move(f));
    }
  };
}
#endif

}
