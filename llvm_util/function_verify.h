#pragma once

// Copyright (c) 2022-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "cache/cache.h"
#include "llvm_util/llvm2alive.h"
#include "smt/smt.h"
#include "tools/transform.h"

#include "llvm/Analysis/TargetLibraryInfo.h"

#include <iostream>
#include <sstream>

namespace verifier {

extern std::optional<smt::smt_initializer> smt_init;
extern std::unique_ptr<Cache> cache;

extern unsigned num_correct;
extern unsigned num_unsound;
extern unsigned num_failed;
extern unsigned num_errors;


bool compareFunctions(llvm::Function &F1, llvm::Function &F2,
                      llvm::TargetLibraryInfoWrapperPass &TLI);


void initialize_verifier(bool set_opt_quiet, bool set_opt_always_verify,
                         bool set_opt_print_dot, bool set_opt_bidirectional,
                         std::ostream *set_out);

}