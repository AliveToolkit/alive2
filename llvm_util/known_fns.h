#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include <memory>
#include <vector>
#include <functional>

namespace llvm {
class CallInst;
class Function;
class TargetLibraryInfo;
}

namespace IR {
class BasicBlock;
class FnAttrs;
class Instr;
class ParamAttrs;
class Value;
}

namespace llvm_util {

// returns true if it's a known function call
bool llvm_implict_attrs(llvm::Function &f, const llvm::TargetLibraryInfo &TLI,
                        IR::FnAttrs &attrs,
                        std::vector<IR::ParamAttrs> &param_attrs,
                        const std::vector<IR::Value*> &args);

// returns true if it's a known function call
std::pair<std::unique_ptr<IR::Instr>, bool>
known_call(llvm::CallInst &i, std::string &&value_name,
           const llvm::TargetLibraryInfo &TLI, IR::BasicBlock &BB,
           const std::vector<IR::Value*> &args, IR::FnAttrs &&attrs,
           std::vector<IR::ParamAttrs> &param_attr,
           std::function<IR::Value*(uint64_t val, int bits)> make_intconst);

}
