#include "llvm_util/compare.h"
#include "llvm_util/llvm2alive.h"
#include "llvm_util/llvm_optimizer.h"
#include "smt/smt.h"
#include "tools/transform.h"
#include "util/version.h"

#include <sstream>
#include <utility>

using namespace tools;
using namespace util;
using namespace std;
using namespace llvm_util;

namespace {

llvm::ExitOnError ExitOnErr;

struct Results {
  Transform t;
  string error;
  Errors errs;
  enum {
    ERROR,
    TYPE_CHECKER_FAILED,
    SYNTACTIC_EQ,
    CORRECT,
    UNSOUND,
    FAILED_TO_PROVE
  } status;

  static Results Error(string &&err) {
    Results r;
    r.status = ERROR;
    r.error = std::move(err);
    return r;
  }
};

Results verify(llvm::Function &F1, llvm::Function &F2,
               llvm::TargetLibraryInfoWrapperPass &TLI,
               smt::smt_initializer &smt_init, ostream *out,
               bool print_transform = false, bool always_verify = false) {
  auto fn1 = llvm2alive(F1, TLI.getTLI(F1), true);
  if (!fn1)
    return Results::Error("Could not translate '" + F1.getName().str() +
                          "' to Alive IR\n");

  auto fn2 = llvm2alive(F2, TLI.getTLI(F2), false, fn1->getGlobalVarNames());
  if (!fn2)
    return Results::Error("Could not translate '" + F2.getName().str() +
                          "' to Alive IR\n");

  Results r;
  r.t.src = std::move(*fn1);
  r.t.tgt = std::move(*fn2);

  if (!always_verify) {
    stringstream ss1, ss2;
    r.t.src.print(ss1);
    r.t.tgt.print(ss2);
    if (std::move(ss1).str() == std::move(ss2).str()) {
      if (print_transform)
        r.t.print(*out, {});
      r.status = Results::SYNTACTIC_EQ;
      return r;
    }
  }

  smt_init.reset();
  r.t.preprocess();
  TransformVerify verifier(r.t, false);

  if (print_transform)
    r.t.print(*out, {});

  {
    auto types = verifier.getTypings();
    if (!types) {
      r.status = Results::TYPE_CHECKER_FAILED;
      return r;
    }
    assert(types.hasSingleTyping());
  }

  r.errs = verifier.verify();
  if (r.errs) {
    r.status = r.errs.isUnsound() ? Results::UNSOUND : Results::FAILED_TO_PROVE;
  } else {
    r.status = Results::CORRECT;
  }
  return r;
}

} // namespace

bool compareFunctions(llvm::Function &F1, llvm::Function &F2,
                      llvm::TargetLibraryInfoWrapperPass &TLI,
                      smt::smt_initializer &smt_init, tv_stats &stats,
                      const tv_opts &opts, ostream *out) {
  auto r = verify(F1, F2, TLI, smt_init, out, !opts.quiet, opts.always_verify);
  if (r.status == Results::ERROR) {
    *out << "ERROR: " << r.error;
    ++stats.num_errors;
    return true;
  }

  if (opts.print_dot) {
    r.t.src.writeDot("src");
    r.t.tgt.writeDot("tgt");
  }

  switch (r.status) {
  case Results::ERROR:
    UNREACHABLE();
    break;

  case Results::SYNTACTIC_EQ:
    *out << "Transformation seems to be correct! (syntactically equal)\n\n";
    ++stats.num_correct;
    break;

  case Results::CORRECT:
    *out << "Transformation seems to be correct!\n\n";
    ++stats.num_correct;
    break;

  case Results::TYPE_CHECKER_FAILED:
    *out << "Transformation doesn't verify!\n"
            "ERROR: program doesn't type check!\n\n";
    ++stats.num_errors;
    return true;

  case Results::UNSOUND:
    *out << "Transformation doesn't verify!\n\n";
    if (!opts.quiet)
      *out << r.errs << endl;
    ++stats.num_unsound;
    return false;

  case Results::FAILED_TO_PROVE:
    *out << r.errs << endl;
    ++stats.num_failed;
    return true;
  }

  if (opts.bidirectional) {
    r = verify(F2, F1, TLI, smt_init, out, false, opts.always_verify);
    switch (r.status) {
    case Results::ERROR:
    case Results::TYPE_CHECKER_FAILED:
      UNREACHABLE();
      break;

    case Results::SYNTACTIC_EQ:
    case Results::CORRECT:
      *out << "These functions seem to be equivalent!\n\n";
      return true;

    case Results::FAILED_TO_PROVE:
      *out << "Failed to verify the reverse transformation\n\n";
      if (!opts.quiet)
        *out << r.errs << endl;
      return true;

    case Results::UNSOUND:
      *out << "Reverse transformation doesn't verify!\n\n";
      if (!opts.quiet)
        *out << r.errs << endl;
      return false;
    }
  }
  return true;
}
