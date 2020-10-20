// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "llvm_util/known_fns.h"
#include "llvm_util/utils.h"
#include "ir/function.h"
#include "ir/instr.h"
#include "util/config.h"
#include "llvm/IR/Constants.h"
#include "llvm/Analysis/MemoryBuiltins.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include <vector>

using namespace IR;
using namespace std;

#define RETURN_KNOWN(op)    return { op, FnKnown }
#define RETURN_FAIL_KNOWN() return { nullptr, FnKnown }
#define RETURN_FAIL_DEPENDS() return { nullptr, FnDependsOnOpt }
#define RETURN_FAIL_UNKNOWN() return { nullptr, FnUnknown }

namespace llvm_util {

pair<unique_ptr<Instr>, KnownFnKind>
known_call(llvm::CallInst &i, const llvm::TargetLibraryInfo &TLI,
           BasicBlock &BB, const vector<Value*> &args) {
  auto ty = llvm_type2alive(i.getType());
  if (!ty)
    RETURN_FAIL_UNKNOWN();

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
    RETURN_FAIL_UNKNOWN();

  if (util::config::io_nobuiltin) {
    switch (libfn) {
    case llvm::LibFunc_printf:
    case llvm::LibFunc_putc:
    case llvm::LibFunc_putchar:
    case llvm::LibFunc_puts:
    case llvm::LibFunc_scanf:
    case llvm::LibFunc_fclose:
    case llvm::LibFunc_ferror:
    case llvm::LibFunc_fgetc:
    case llvm::LibFunc_fprintf:
    case llvm::LibFunc_fputc:
    case llvm::LibFunc_fputs:
    case llvm::LibFunc_fread:
    case llvm::LibFunc_fscanf:
    case llvm::LibFunc_fwrite:
    case llvm::LibFunc_lstat:
    case llvm::LibFunc_perror:
    case llvm::LibFunc_read:
    case llvm::LibFunc_write:
      RETURN_FAIL_DEPENDS();
    default:
      break;
    }
  }

  switch (libfn) {
  case llvm::LibFunc_memset: // void* memset(void *ptr, int val, size_t bytes)
    BB.addInstr(make_unique<Memset>(*args[0], *args[1], *args[2], 1));
    RETURN_KNOWN(make_unique<UnaryOp>(*ty, value_name(i), *args[0],
                                      UnaryOp::Copy));
  case llvm::LibFunc_strlen:
    if (auto BI = llvm::dyn_cast<llvm::BitCastInst>(i.getArgOperand(0))) {
      auto G = llvm::dyn_cast<llvm::GlobalVariable>(BI->getOperand(0));
      if (G && G->isConstant() && G->hasDefinitiveInitializer()) {
        auto C = llvm::dyn_cast<llvm::ConstantDataArray>(G->getInitializer());
        if (C && C->isCString()) {
          RETURN_KNOWN(make_unique<UnaryOp>(*ty, value_name(i),
              *make_intconst(C->getAsCString().size(), ty->bits()),
              UnaryOp::Copy));
        }
      }
    }
    RETURN_KNOWN(make_unique<Strlen>(*ty, value_name(i), *args[0]));
  case llvm::LibFunc_memcmp:
  case llvm::LibFunc_bcmp: {
    RETURN_KNOWN(
      make_unique<Memcmp>(*ty, value_name(i), *args[0], *args[1], *args[2],
                          libfn == llvm::LibFunc_bcmp));
  }
  case llvm::LibFunc_ffs:
  case llvm::LibFunc_ffsl:
  case llvm::LibFunc_ffsll: {
    bool needs_trunc = &args[0]->getType() != ty;
    auto *Op = new UnaryOp(args[0]->getType(),
                           value_name(i) + (needs_trunc ? "#beftrunc" : ""),
                           *args[0], UnaryOp::FFS);
    if (!needs_trunc)
      RETURN_KNOWN(unique_ptr<UnaryOp>(Op));

    BB.addInstr(unique_ptr<UnaryOp>(Op));
    RETURN_KNOWN(
      make_unique<ConversionOp>(*ty, value_name(i), *Op, ConversionOp::Trunc));
  }
  default:
    RETURN_FAIL_KNOWN();
  }
}

}
