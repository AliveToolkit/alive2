#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include <llvm/ADT/StringRef.h>

namespace llvm {
class Module;
}

namespace llvm_util {
void optimize_module(llvm::Module *M, llvm::StringRef optArgs);
}
