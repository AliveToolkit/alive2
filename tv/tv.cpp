// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "llvm_util/llvm2alive.h"
#include "smt/smt.h"
#include "smt/solver.h"
#include "tools/transform.h"
#include "util/config.h"
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
#include <fstream>
#include <iostream>
#include <random>
#include <unordered_map>
#include <utility>

#if (__GNUC__ < 8) && (!__APPLE__)
# include <experimental/filesystem>
  namespace fs = std::experimental::filesystem;
#else
# include <filesystem>
  namespace fs = std::filesystem;
#endif

using namespace IR;
using namespace llvm_util;
using namespace tools;
using namespace util;
using namespace std;

namespace {

llvm::cl::opt<bool> opt_error_fatal(
  "tv-exit-on-error", llvm::cl::desc("Alive: exit on error"),
  llvm::cl::init(false));

llvm::cl::opt<unsigned> opt_smt_to(
  "tv-smt-to", llvm::cl::desc("Alive: timeout for SMT queries"),
  llvm::cl::init(1000), llvm::cl::value_desc("ms"));

llvm::cl::opt<unsigned> opt_max_mem(
  "tv-max-mem", llvm::cl::desc("Alive: max memory (aprox)"),
  llvm::cl::init(1024), llvm::cl::value_desc("MB"));

llvm::cl::opt<bool> opt_se_verbose(
  "tv-se-verbose", llvm::cl::desc("Alive: symbolic execution verbose mode"),
  llvm::cl::init(false));

llvm::cl::opt<bool> opt_smt_stats(
  "tv-smt-stats", llvm::cl::desc("Alive: show SMT statistics"),
  llvm::cl::init(false));

llvm::cl::opt<bool> opt_smt_skip(
  "tv-smt-skip", llvm::cl::desc("Alive: skip SMT queries"),
  llvm::cl::init(false));

llvm::cl::opt<string> opt_report_dir(
  "tv-report-dir", llvm::cl::desc("Alive: save report to disk"),
  llvm::cl::value_desc("directory"));

llvm::cl::opt<bool> opt_smt_verbose(
  "tv-smt-verbose", llvm::cl::desc("Alive: SMT verbose mode"),
  llvm::cl::init(false));

llvm::cl::opt<bool> opt_tactic_verbose(
  "tv-tactic-verbose", llvm::cl::desc("Alive: SMT Tactic verbose mode"),
  llvm::cl::init(false));

llvm::cl::opt<bool> opt_print_dot(
  "tv-dot", llvm::cl::desc("Alive: print .dot file with CFG of each function"),
  llvm::cl::init(false));

llvm::cl::opt<bool> opt_disable_poison_input(
  "tv-disable-poison-input",
  llvm::cl::desc("Alive: Assume function input cannot be poison"),
  llvm::cl::init(false));

llvm::cl::opt<bool> opt_disable_undef_input(
  "tv-disable-undef-input",
  llvm::cl::desc("Alive: Assume function input cannot be undef"),
  llvm::cl::init(false));

llvm::cl::list<std::string> opt_funcs(
  "tv-func",
  llvm::cl::desc("Name of functions to verify (without @)"),
  llvm::cl::ZeroOrMore, llvm::cl::value_desc("function name"));

llvm::cl::opt<bool> opt_debug(
  "tv-dbg",
  llvm::cl::desc("Alive: Show debug data"),
  llvm::cl::init(false), llvm::cl::Hidden);

llvm::cl::opt<unsigned> opt_omit_array_size(
  "tv-omit-array-size",
  llvm::cl::desc("Omit an array initializer if it has elements more than "
                  "this number"),
  llvm::cl::init(-1));

llvm::cl::opt<bool> opt_io_nobuiltin(
    "tv-io-nobuiltin",
    llvm::cl::desc("Encode standard I/O functions as an unknown function"),
    llvm::cl::init(false));

ostream *out;
ofstream out_file;
string report_filename;
optional<smt::smt_initializer> smt_init;
optional<llvm_util::initializer> llvm_util_init;
TransformPrintOpts print_opts;
unordered_map<string, pair<Function, unsigned>> fns;
set<string> fnsToVerify;
unsigned initialized = 0;
bool showed_stats = false;
bool report_dir_created = false;
bool has_failure = false;
bool is_clangtv = false;


struct TVPass final : public llvm::FunctionPass {
  static char ID;

  TVPass() : FunctionPass(ID) {}

  bool runOnFunction(llvm::Function &F) override {
    if (F.isDeclaration())
      // This can happen at EntryExitInstrumenter pass.
      return false;

    if (!fnsToVerify.empty() && !fnsToVerify.count(F.getName().str()))
      return false;

    llvm::TargetLibraryInfo *TLI = nullptr;
    unique_ptr<llvm::TargetLibraryInfo> TLI_holder;
    if (is_clangtv) {
      // When used as a clang plugin, this is run as a plain function rather
      // than a registered pass, so getAnalysis() cannot be used.
      TLI_holder
        = make_unique<llvm::TargetLibraryInfo>(llvm::TargetLibraryInfoImpl(
           llvm::Triple(F.getParent()->getTargetTriple())), &F);
      TLI = TLI_holder.get();
    } else {
      TLI = &getAnalysis<llvm::TargetLibraryInfoWrapperPass>().getTLI(F);
    }

    auto [I, first] = fns.try_emplace(F.getName().str());
    auto fn = llvm2alive(F, *TLI, first ? vector<string_view>()
                                        : I->second.first.getGlobalVarNames());
    if (!fn) {
      fns.erase(I);
      return false;
    }

    auto old_fn = move(I->second.first);
    I->second.first = move(*fn);

    if (opt_print_dot) {
      auto &f = I->second.first;
      ofstream file(f.getName() + '.' + to_string(I->second.second) + ".dot");
      CFG cfg(f);
      cfg.printDot(file);
      ofstream fileDom(f.getName() + '.' + to_string(I->second.second++) +
                       ".dom.dot");
      DomTree(f, cfg).printDot(fileDom);
    }

    if (first)
      return false;

    smt_init->reset();
    Transform t;
    t.src = move(old_fn);
    t.tgt = move(I->second.first);
    TransformVerify verifier(t, false);
    t.print(*out, print_opts);

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

    I->second.first = move(t.tgt);
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

      do {
        auto newname = fname.stem();
        newname += "_" + to_string(rand(re)) + ".txt";
        path.replace_filename(newname);
      } while (fs::exists(path));

      out_file = ofstream(path);
      out = &out_file;
      if (!out_file.is_open()) {
        cerr << "Alive2: Couldn't open report file!" << endl;
        exit(1);
      }

      report_filename = path;
      *out << "Source: " << source_file << endl;
      report_dir_created = true;
    } else if (opt_report_dir.empty())
      out = &cerr;

    showed_stats = false;
    smt::solver_print_queries(opt_smt_verbose);
    smt::solver_tactic_verbose(opt_tactic_verbose);
    smt::set_query_timeout(to_string(opt_smt_to));
    smt::set_memory_limit(opt_max_mem * 1024 * 1024);
    config::skip_smt = opt_smt_skip;
    config::io_nobuiltin = opt_io_nobuiltin;
    config::symexec_print_each_value = opt_se_verbose;
    config::disable_undef_input = opt_disable_undef_input;
    config::disable_poison_input = opt_disable_poison_input;
    config::debug = opt_debug;
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
      if (has_failure && !report_filename.empty())
        cerr << "Report written to " << report_filename << endl;
    }

    llvm_util_init.reset();
    smt_init.reset();
    --initialized;

    if (has_failure) {
      cerr << "Alive2: Transform doesn't verify; aborting!" << endl;
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
    "GlobalOptPass", "IPSCCPPass"
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
        [](llvm::ModulePassManager &MPM) { MPM.addPass(TVInitPass()); }
      );
      PB.registerOptimizerLastEPCallback(
          [](llvm::ModulePassManager &MPM,
             llvm::PassBuilder::OptimizationLevel) {
            MPM.addPass(createModuleToFunctionPassAdaptor(TVFinalizePass()));
          });
      auto f = [](llvm::StringRef P, llvm::Any IR) {
        static int count = 0;
        if (!out) {
          // TVInitPass is not called yet.
          // This can happen at very early passes, such as
          // ForceFunctionAttrsPass.
          return;
        }

        if (do_skip(P)) {
          *out << "-- " << ++count << ". " << P.str() << " : Skipping\n";
          return;
        } else if (TVFinalizePass::finalized)
          return;

        *out << "-- " << ++count << ". " << P.str() << "\n";
        TVPass tv;
        auto M = const_cast<llvm::Module *>(unwrapModule(IR));
        for (auto &F: *M)
          tv.runOnFunction(F);
      };
      PB.getPassInstrumentationCallbacks()->registerAfterPassCallback(move(f));
    }
  };
}
#endif

}
