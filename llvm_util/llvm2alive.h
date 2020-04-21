#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/function.h"
#include <optional>
#include <ostream>
#include <string>
#include <vector>

namespace llvm {
class DataLayout;
class Function;
class TargetLibraryInfo;
}

namespace llvm_util {

// The maximum number of elements of an array initializer to explicitly store.
// If longer, the initializer will simply be omitted.
extern unsigned omit_array_size;


struct initializer {
  initializer(std::ostream &os, const llvm::DataLayout &DL);
};

std::optional<IR::Function> llvm2alive(llvm::Function &F,
    const llvm::TargetLibraryInfo &TLI,
    const std::vector<std::string_view> &gvnamesInSrc = {});
}
