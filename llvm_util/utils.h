#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/instr.h"
#include <functional>
#include <ostream>
#include <string>

namespace llvm {
class APInt;
class BasicBlock;
class ConstantExpr;
class DataLayout;
class Instruction;
class Type;
class Value;
class Module;
class LLVMContext;
class Function;
}

namespace IR {
class AggregateValue;
class BasicBlock;
class Function;
class Type;
class Value;
}

namespace llvm_util {

IR::FastMathFlags parse_fmath(llvm::Instruction &i);

IR::Type& get_int_type(unsigned bits);
IR::Type* llvm_type2alive(const llvm::Type *ty);

#define PRINT(T) std::ostream& operator<<(std::ostream &os, const T &x);
PRINT(llvm::Type)
PRINT(llvm::Value)
#undef PRINT

void init_llvm_utils(std::ostream &os, const llvm::DataLayout &DL);

std::ostream& get_outs();
void set_outs(std::ostream &os);

bool hasOpaqueType(llvm::Type *ty);

std::unique_ptr<llvm::Module> openInputFile(llvm::LLVMContext &Context,
                                            const std::string &InputFilename);
llvm::Function *findFunction(llvm::Module &M, const std::string &FName);

}
