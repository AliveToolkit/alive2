// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#ifndef LLVM_BINOP
# define LLVM_BINOP(x,y)
#endif

LLVM_BINOP(llvm::Instruction::Add,  BinOp::Add)
LLVM_BINOP(llvm::Instruction::Sub,  BinOp::Sub)
LLVM_BINOP(llvm::Instruction::Mul,  BinOp::Mul)
LLVM_BINOP(llvm::Instruction::SDiv, BinOp::SDiv)
LLVM_BINOP(llvm::Instruction::UDiv, BinOp::UDiv)
LLVM_BINOP(llvm::Instruction::SRem, BinOp::SRem)
LLVM_BINOP(llvm::Instruction::URem, BinOp::URem)  
LLVM_BINOP(llvm::Instruction::Shl,  BinOp::Shl)
LLVM_BINOP(llvm::Instruction::AShr, BinOp::AShr)
LLVM_BINOP(llvm::Instruction::LShr, BinOp::LShr)
LLVM_BINOP(llvm::Instruction::And,  BinOp::And)
LLVM_BINOP(llvm::Instruction::Or,   BinOp::Or)
LLVM_BINOP(llvm::Instruction::Xor , BinOp::Xor)
