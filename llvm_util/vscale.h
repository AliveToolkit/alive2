#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "smt/expr.h"

#include "llvm/IR/Function.h"

#include <optional>
#include <vector>

namespace llvm_util {

/// True if anything reachable from F makes its semantics depend on vscale:
/// a scalable type or llvm.vscale. A vscale_range attribute by itself does
/// not count.
bool referencesVScale(const llvm::Function &F);

/// The constraint that vscale lies within F's vscale_range attribute, if it
/// has one.
smt::expr vscaleInRange(const llvm::Function &F, const smt::expr &vscale);

/// Enumerate the power-of-two vscales in [min, max] that src's vscale_range
/// admits (either function's, when bidirectional), sorted in ascending order.
/// No solver objects survive this call, so callers can reset the SMT context
/// while verifying each concrete assignment.
/// Returns nullopt if enumeration fails.
std::optional<std::vector<unsigned>>
getVScales(const llvm::Function &src, const llvm::Function &tgt,
           bool bidirectional, unsigned min, unsigned max);

}
