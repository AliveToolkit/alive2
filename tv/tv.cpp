// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "smt/smt.h"
#include "smt/solver.h"
#include "tools/transform.h"
#include "tv/llvm-utils.h"
#include "util/config.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"
#include <fstream>
#include <iostream>
#include <optional>
#include <utility>
#include <vector>

#if __GNUC__ < 8
# include <experimental/filesystem>
  namespace fs = std::experimental::filesystem;
#else
# include <filesystem>
  namespace fs = std::filesystem;
#endif

using namespace IR;
using namespace tools;
using namespace util;
using namespace std;
using llvm::cast, llvm::dyn_cast, llvm::isa, llvm::errs;
using llvm::LLVMContext;

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

#if 0
string_view s(llvm::StringRef str) {
  return { str.data(), str.size() };
}
#endif

ostream *out;
ofstream out_file;
optional<smt::smt_initializer> smt_init;
TransformPrintOpts print_opts;
unordered_map<string, pair<Function, unsigned>> fns;

struct TVPass : public llvm::FunctionPass {
  static char ID;

  TVPass() : FunctionPass(ID) {}

  bool runOnFunction(llvm::Function &F) override {
    auto fn = llvm2alive(F, out).run();
    if (!fn) {
      fns.erase(F.getName());
      return false;
    }

    auto [old_fn, inserted] = fns.try_emplace(fn->getName(), move(*fn), 0);

    if (opt_print_dot) {
      auto &f = inserted ? old_fn->second.first : *fn;
      ofstream file(f.getName() + '.' + to_string(old_fn->second.second++)
                      + ".dot");
      CFG(f).printDot(file);
    }

    if (inserted)
      return false;

    smt_init->reset();
    Transform t;
    t.src = move(old_fn->second.first);
    t.tgt = move(*fn);
    TransformVerify verifier(t, false);
    t.print(*out, print_opts);

    if (Errors errs = verifier.verify()) {
      *out << "Transformation doesn't verify!\n" << errs << endl;
      if (opt_error_fatal && !errs.isTimeout())
        llvm::report_fatal_error("Alive2: Transform doesn't verify; aborting!");
    } else {
      *out << "Transformation seems to be correct!\n\n";
    }

    old_fn->second.first = move(t.tgt);
    return false;
  }

  bool doInitialization(llvm::Module &module) override {
    static bool done = false;
    if (done)
      return false;
    done = true;

    if (!opt_report_dir.empty()) {
      // TODO: make dir if it doesn't exist
      auto &source_file = module.getSourceFileName();
      fs::path fname = source_file.empty() ? "alive.txt" : source_file;
      fname.replace_extension(".txt");
      fs::path path = fs::path(opt_report_dir.getValue()) / fname.filename();

      unsigned n = 0;
      while (fs::exists(path)) {
        auto newname = fname.stem();
        newname += "_" + to_string(++n) + ".txt";
        path.replace_filename(newname);
      }

      out_file = ofstream(path);
      out = &out_file;
      if (!out_file.is_open())
        llvm::report_fatal_error("Alive2: Couldn't open report file!");

      *out << "Source: " << source_file << endl;
    } else
      out = &cerr;

    initLLVMUtils();
    
    smt::solver_print_queries(opt_smt_verbose);
    smt::solver_tactic_verbose(opt_tactic_verbose);
    smt::set_query_timeout(to_string(opt_smt_to));
    smt::set_memory_limit(opt_max_mem * 1024 * 1024);
    config::skip_smt = opt_smt_skip;
    config::symexec_print_each_value = opt_se_verbose;

    smt_init.emplace();
    return false;
  }

  bool doFinalization(llvm::Module&) override {
    static bool showed_stats = false;
    if (opt_smt_stats && !showed_stats) {
      smt::solver_print_stats(*out);
      showed_stats = true;
    }
    smt_init.reset();
    finalizeLLVMUtils();
    return false;
  }

  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override {
    AU.setPreservesAll();
  }
};

char TVPass::ID = 0;
llvm::RegisterPass<TVPass> X("tv", "Translation Validator", false, false);

}
