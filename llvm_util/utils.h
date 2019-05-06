#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/function.h"
#include <optional>
#include <ostream>

namespace llvm { class Function; }

namespace llvm_util {

struct initializer {
  initializer(std::ostream &os);
  ~initializer();
};

std::optional<IR::Function> llvm2alive(llvm::Function &F);

}
