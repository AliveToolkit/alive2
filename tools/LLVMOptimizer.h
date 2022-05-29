#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/Error.h"

void optimize_module(llvm::StringRef optArgs, llvm::Module *M);
