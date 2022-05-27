#pragma once
#include "llvm/IR/Function.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/Error.h"

#include <string>


llvm::Module *optimize_module(std::string optArgs,llvm::Module *M);
llvm::Function *optimize_function(std::string optArgs, llvm::Function *func);
