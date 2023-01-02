#include "smt/smt.h"

#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/IR/Function.h"

struct tv_stats {
  unsigned num_correct, num_unsound, num_failed, num_errors;
};

struct tv_opts {
  tv_opts(bool quiet, bool always_verify, bool print_dot, bool bidirectional)
      : quiet(quiet), always_verify(always_verify), print_dot(print_dot),
        bidirectional(bidirectional) {}
  bool quiet, always_verify, print_dot, bidirectional;
};

bool compareFunctions(llvm::Function &F1, llvm::Function &F2,
                      llvm::TargetLibraryInfoWrapperPass &TLI,
                      smt::smt_initializer &smt_init, tv_stats &stats,
                      const tv_opts &opts, std::ostream *out);
