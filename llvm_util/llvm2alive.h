#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/function.h"
#include <optional>
#include <ostream>

namespace llvm {
class Function;
class TargetLibraryInfo;
}

namespace llvm_util {

struct initializer {
  initializer(std::ostream &os);
};

std::optional<IR::Function> llvm2alive(llvm::Function &F,
                                       const llvm::TargetLibraryInfo &TLI);
}
