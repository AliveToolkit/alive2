#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include <string>
#include <string_view>

namespace llvm {
class Module;
}

namespace llvm_util {
std::string optimize_module(llvm::Module &M, std::string_view optArgs);
}
