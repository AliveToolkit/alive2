// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "llvm_util/compare.h"
#include "llvm_util/llvm2alive.h"
#include "llvm_util/llvm_optimizer.h"
#include "smt/smt.h"
#include "tools/transform.h"
#include "util/config.h"

#include <sstream>
#include <utility>

using namespace tools;
using namespace util;
using namespace std;
using namespace llvm_util;

namespace {

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
               smt::smt_initializer &smt_init, ostream &out,
               bool print_transform, bool always_verify) {
  auto fn1 = llvm2alive(F1, TLI.getTLI(F1), true);
  if (!fn1)
    return Results::Error("Could not translate '" + F1.getName().str() +
                          "' to Alive IR\n");

  auto fn2 = llvm2alive(F2, TLI.getTLI(F2), false, fn1->getGlobalVars());
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
        r.t.print(out, {});
      r.status = Results::SYNTACTIC_EQ;
      return r;
    }
  }

  smt_init.reset();
  r.t.preprocess();
  TransformVerify verifier(r.t, false);

  if (print_transform)
    r.t.print(out, {});

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

bool Verifier::compareFunctions(llvm::Function &F1, llvm::Function &F2) {
  auto r = verify(F1, F2, TLI, smt_init, out, !config::quiet, always_verify);
  if (r.status == Results::ERROR) {
    out << "ERROR: " << r.error;
    ++num_errors;
    return true;
  }

  if (print_dot) {
    r.t.src.writeDot("src");
    r.t.tgt.writeDot("tgt");
  }

  if (r.errs.hasWarnings())
    r.errs.printWarnings(out);

  switch (r.status) {
  case Results::ERROR:
    UNREACHABLE();
    break;

  case Results::SYNTACTIC_EQ:
    out << "Transformation seems to be correct! (syntactically equal)\n\n";
    ++num_correct;
    break;

  case Results::CORRECT:
    out << "Transformation seems to be correct!\n\n";
    ++num_correct;
    break;

  case Results::TYPE_CHECKER_FAILED:
    out << "Transformation doesn't verify!\n"
            "ERROR: program doesn't type check!\n\n";
    ++num_errors;
    return true;

  case Results::UNSOUND:
    out << "Transformation doesn't verify!\n\n";
    if (!config::quiet)
      out << r.errs << endl;
    ++num_unsound;
    return false;

  case Results::FAILED_TO_PROVE:
    out << r.errs << endl;
    ++num_failed;
    return true;
  }

  if (bidirectional) {
    r = verify(F2, F1, TLI, smt_init, out, false, always_verify);
    switch (r.status) {
    case Results::ERROR:
    case Results::TYPE_CHECKER_FAILED:
      UNREACHABLE();
      break;

    case Results::SYNTACTIC_EQ:
    case Results::CORRECT:
      out << "These functions seem to be equivalent!\n\n";
      return true;

    case Results::FAILED_TO_PROVE:
      out << "Failed to verify the reverse transformation\n\n";
      if (!config::quiet)
        out << r.errs << endl;
      return true;

    case Results::UNSOUND:
      out << "Reverse transformation doesn't verify!\n\n";
      if (!config::quiet)
        out << r.errs << endl;
      return false;
    }
  }
  return true;
}
