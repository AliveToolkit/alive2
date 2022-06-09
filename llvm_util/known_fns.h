#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/attrs.h"
#include <memory>
#include <tuple>
#include <vector>

namespace llvm {
class CallInst;
class Function;
class TargetLibraryInfo;
}

namespace IR {
class BasicBlock;
class Instr;
class Value;
}

namespace llvm_util {

// returned bool indicates whether it's a known function call
std::pair<std::vector<IR::ParamAttrs>, bool>
llvm_implict_attrs(llvm::Function &f, const llvm::TargetLibraryInfo &TLI,
                   IR::FnAttrs &attrs);

// returned bool indicates whether it's a known function call
std::tuple<std::unique_ptr<IR::Instr>, IR::FnAttrs, std::vector<IR::ParamAttrs>,
           bool>
known_call(llvm::CallInst &i, const llvm::TargetLibraryInfo &TLI,
           IR::BasicBlock &BB, const std::vector<IR::Value*> &args);

}
