#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "llvm/IR/InstVisitor.h"

void initLLVMUtils();
void finalizeLLVMUtils();

class llvm2alive : public llvm::InstVisitor<llvm2alive, std::unique_ptr<IR::Instr>> {
  llvm::Function &f;
  std::ostream *out;

public:
  llvm2alive(llvm::Function &f, std::ostream *out) : f(f), out(out) {}
  using RetTy = std::unique_ptr<IR::Instr>;
  RetTy visitBinaryOperator(llvm::BinaryOperator &i);
  RetTy visitSExtInst(llvm::SExtInst &i);
  RetTy visitZExtInst(llvm::ZExtInst &i);
  RetTy visitTruncInst(llvm::TruncInst &i);
  RetTy visitCallInst(llvm::CallInst &i);
  RetTy visitICmpInst(llvm::ICmpInst &i);
  RetTy visitSelectInst(llvm::SelectInst &i);
  RetTy visitExtractElementInst(llvm::ExtractElementInst &i);
  RetTy visitPHINode(llvm::PHINode &i);
  RetTy visitBranchInst(llvm::BranchInst &i);
  RetTy visitSwitchInst(llvm::SwitchInst &i);
  RetTy visitReturnInst(llvm::ReturnInst &i);
  RetTy visitUnreachableInst(llvm::UnreachableInst &i);
  RetTy visitIntrinsicInst(llvm::IntrinsicInst &i);
  RetTy visitDbgInfoIntrinsic(llvm::DbgInfoIntrinsic&) { return {}; }
  RetTy visitInstruction(llvm::Instruction &i) { return error(i); }
  RetTy error(llvm::Instruction &i);
  bool handleMetadata(llvm::Instruction &llvm_i, IR::Instr &i, IR::BasicBlock &BB);
  std::optional<IR::Function> run();
};

