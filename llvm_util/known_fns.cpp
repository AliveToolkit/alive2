// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "llvm_util/known_fns.h"
#include "llvm_util/utils.h"
#include "ir/function.h"
#include "ir/instr.h"
#include "llvm/IR/Constants.h"
#include "llvm/Analysis/MemoryBuiltins.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include <vector>

using namespace IR;
using namespace std;

#define RETURN_KNOWN(op)    return { op, true }
#define RETURN_FAIL_KNOWN() return { nullptr, true }

namespace llvm_util {

pair<unique_ptr<Instr>, bool>
known_call(llvm::CallInst &i, const llvm::TargetLibraryInfo &TLI,
           BasicBlock &BB, const vector<Value*> &args) {
  auto ty = llvm_type2alive(i.getType());
  if (!ty)
    RETURN_FAIL_KNOWN();

  // TODO: add support for checking mismatch of C vs C++ alloc fns
  if (llvm::isMallocLikeFn(&i, &TLI, false)) {
    bool isNonNull = i.getCalledFunction()->getName() != "malloc";
    RETURN_KNOWN(make_unique<Malloc>(*ty, value_name(i), *args[0], isNonNull));
  } else if (llvm::isCallocLikeFn(&i, &TLI, false)) {
    RETURN_KNOWN(make_unique<Calloc>(*ty, value_name(i), *args[0], *args[1]));
  } else if (llvm::isReallocLikeFn(&i, &TLI, false)) {
    RETURN_KNOWN(make_unique<Malloc>(*ty, value_name(i), *args[0], *args[1]));
  } else if (llvm::isFreeCall(&i, &TLI)) {
    RETURN_KNOWN(make_unique<Free>(*args[0]));
  }

  auto decl = i.getCalledFunction();
  llvm::LibFunc libfn;
  if (!decl || !TLI.getLibFunc(*decl, libfn) || !TLI.has(libfn))
    return { nullptr, false };

  switch (libfn) {
  case llvm::LibFunc_memset: // void* memset(void *ptr, int val, size_t bytes)
    BB.addInstr(make_unique<Memset>(*args[0], *args[1], *args[2], 1));
    RETURN_KNOWN(make_unique<UnaryOp>(*ty, value_name(i), *args[0],
                                      UnaryOp::Copy));
  case llvm::LibFunc_strlen:
    if (llvm::isa<llvm::BitCastInst>(i.getArgOperand(0))) {
      auto BI = llvm::dyn_cast<llvm::BitCastInst>(i.getArgOperand(0));
      auto G = llvm::dyn_cast<llvm::GlobalVariable>(BI->getOperand(0));
      if (G && G->isConstant() && G->hasInitializer()) {
        auto C = llvm::dyn_cast<llvm::ConstantDataArray>(G->getInitializer());
        if (C && C->isCString()) {
          auto sz = llvm::ConstantInt::get(i.getType(), C->getAsCString().size());
          RETURN_KNOWN(make_unique<UnaryOp>(*ty, value_name(i),
              *get_operand(sz, [](auto x){ return nullptr; },
                               [](auto x){ return nullptr; }),
              UnaryOp::Copy));
        }
      }
    }
    RETURN_KNOWN(make_unique<Strlen>(*ty, value_name(i), *args[0]));
  default:
    RETURN_FAIL_KNOWN();
  }
}

}
