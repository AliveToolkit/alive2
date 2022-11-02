// Copyright (c) 2022-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "function_verify.h"

using namespace tools;
using namespace util;
using namespace std;
using namespace llvm_util;

namespace {
bool opt_quiet = false;
bool opt_always_verify = false;
bool opt_print_dot = false;
bool opt_bidirectional = false;

ostream* out = &cout;

struct Results {
  tools::Transform t;
  std::string error;
  util::Errors errs;
  enum {
    ERROR,
    TYPE_CHECKER_FAILED,
    SYNTACTIC_EQ,
    CORRECT,
    UNSOUND,
    FAILED_TO_PROVE
  } status;

  static Results Error(std::string &&err) {
    Results r;
    r.status = ERROR;
    r.error = std::move(err);
    return r;
  }
};

Results verify(llvm::Function &F1, llvm::Function &F2,
               llvm::TargetLibraryInfoWrapperPass &TLI, bool print_transform,
               bool always_verify) {
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
    std::stringstream ss1, ss2;
    r.t.src.print(ss1);
    r.t.tgt.print(ss2);
    if (std::move(ss1).str() == std::move(ss2).str()) {
      if (print_transform)
        r.t.print(*out, {});
      r.status = Results::SYNTACTIC_EQ;
      return r;
    }
  }

  verifier::smt_init->reset();
  r.t.preprocess();
  tools::TransformVerify verifier(r.t, false);

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
}

namespace verifier {

std::optional<smt::smt_initializer> smt_init;
std::unique_ptr<Cache> cache;

unsigned num_correct = 0;
unsigned num_unsound = 0;
unsigned num_failed = 0;
unsigned num_errors = 0;


void initialize_verifier(bool set_opt_quiet, bool set_opt_always_verify,
                        bool set_opt_print_dot, bool set_opt_bidirectional,
                        std::ostream* set_out) {
  opt_quiet = set_opt_quiet;
  opt_always_verify = set_opt_always_verify;
  opt_print_dot = set_opt_print_dot;
  opt_bidirectional = set_opt_bidirectional;
  out = set_out;
}


bool compareFunctions(llvm::Function &F1, llvm::Function &F2,
                      llvm::TargetLibraryInfoWrapperPass &TLI) {
  auto r = verify(F1, F2, TLI, !opt_quiet, opt_always_verify);
  if (r.status == Results::ERROR) {
    *out << "ERROR: " << r.error;
    ++num_errors;
    return true;
  }

  if (opt_print_dot) {
    r.t.src.writeDot("src");
    r.t.tgt.writeDot("tgt");
  }

  switch (r.status) {
  case Results::ERROR:
    UNREACHABLE();
    break;

  case Results::SYNTACTIC_EQ:
    *out << "Transformation seems to be correct! (syntactically equal)\n\n";
    ++num_correct;
    break;

  case Results::CORRECT:
    *out << "Transformation seems to be correct!\n\n";
    ++num_correct;
    break;

  case Results::TYPE_CHECKER_FAILED:
    *out << "Transformation doesn't verify!\n"
            "ERROR: program doesn't type check!\n\n";
    ++num_errors;
    return true;

  case Results::UNSOUND:
    *out << "Transformation doesn't verify!\n\n";
    if (!opt_quiet)
      *out << r.errs << endl;
    ++num_unsound;
    return false;

  case Results::FAILED_TO_PROVE:
    *out << r.errs << endl;
    ++num_failed;
    return true;
  }

  if (opt_bidirectional) {
    r = verify(F2, F1, TLI, false, opt_always_verify);
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
      if (!opt_quiet)
        *out << r.errs << endl;
      return true;

    case Results::UNSOUND:
      *out << "Reverse transformation doesn't verify!\n\n";
      if (!opt_quiet)
        *out << r.errs << endl;
      return false;
    }
  }
  return true;
}
}

