// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "llvm_util/known_fns.h"
#include "llvm_util/utils.h"
#include "ir/function.h"
#include "ir/instr.h"
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
           BasicBlock &BB) {
  auto ty = llvm_type2alive(i.getType());
  if (!ty)
    RETURN_FAIL_KNOWN();

  vector<Value*> args;
  for (auto &arg : i.args()) {
    auto a = get_operand(arg);
    if (!a)
      RETURN_FAIL_KNOWN();
    args.emplace_back(a);
  }

  // TODO: add support for checking mismatch of C vs C++ alloc fns
  if (llvm::isMallocLikeFn(&i, &TLI, false)) {
    RETURN_KNOWN(make_unique<Malloc>(*ty, value_name(i), *args[0]));
  } else if (llvm::isFreeCall(&i, &TLI)) {
    RETURN_KNOWN(make_unique<Free>(*args[0]));
  }

  auto decl = i.getCalledFunction();
  llvm::LibFunc libfn;
  if (!decl || !TLI.getLibFunc(*decl, libfn))
    return { nullptr, false };

  switch (libfn) {
  case llvm::LibFunc_memset: // void* memset(void *ptr, int val, size_t bytes)
    BB.addInstr(make_unique<Memset>(*args[0], *args[1], *args[2], 1));
    RETURN_KNOWN(make_unique<UnaryOp>(*ty, value_name(i), *args[0],
                                      UnaryOp::Copy));
  default:
    RETURN_FAIL_KNOWN();
  }
}

}
