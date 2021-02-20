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

#define RETURN_VAL(op) \
  return { op, move(attrs), move(param_attrs), false }
#define RETURN_EXACT() \
  return { nullptr, move(attrs), move(param_attrs), false }
#define RETURN_APPROX() \
  return { nullptr, move(attrs), move(param_attrs), true }

namespace llvm_util {

tuple<unique_ptr<Instr>, FnAttrs, vector<ParamAttrs>, bool>
known_call(llvm::CallInst &i, const llvm::TargetLibraryInfo &TLI,
           BasicBlock &BB, const vector<Value*> &args) {
  FnAttrs attrs;
  vector<ParamAttrs> param_attrs;

  auto ty = llvm_type2alive(i.getType());
  if (!ty)
    RETURN_EXACT();

  // TODO: add support for checking mismatch of C vs C++ alloc fns
  if (llvm::isMallocLikeFn(&i, &TLI, false)) {
    bool isNonNull = i.getCalledFunction()->getName() != "malloc";
    RETURN_VAL(make_unique<Malloc>(*ty, value_name(i), *args[0], isNonNull));
  } else if (llvm::isCallocLikeFn(&i, &TLI, false)) {
    RETURN_VAL(make_unique<Calloc>(*ty, value_name(i), *args[0], *args[1]));
  } else if (llvm::isReallocLikeFn(&i, &TLI, false)) {
    RETURN_VAL(make_unique<Malloc>(*ty, value_name(i), *args[0], *args[1]));
  } else if (llvm::isFreeCall(&i, &TLI)) {
    if (i.hasFnAttr(llvm::Attribute::NoFree)) {
      auto zero = make_intconst(0, 1);
      RETURN_VAL(make_unique<Assume>(*zero, Assume::AndNonPoison));
    }
    RETURN_VAL(make_unique<Free>(*args[0]));
  }

  auto set_param = [&](unsigned i, ParamAttrs::Attribute attr) {
    if (param_attrs.size() <= i)
      param_attrs.resize(i+1);
    param_attrs[i].set(attr);
  };

  auto ret_and_args_no_undef = [&]() {
    if (!dynamic_cast<VoidType*>(ty))
      attrs.set(FnAttrs::NoUndef);
    for (unsigned i = 0, e = args.size(); i < e; ++i) {
      set_param(i, ParamAttrs::NoUndef);
    }
  };

  auto fputc_attr = [&]() {
    ret_and_args_no_undef();
    attrs.set(FnAttrs::NoThrow);
    attrs.set(FnAttrs::NoFree);
    set_param(1, ParamAttrs::NoCapture);
  };

  auto decl = i.getCalledFunction();
  llvm::LibFunc libfn;
  if (!decl || !TLI.getLibFunc(*decl, libfn) || !TLI.has(libfn))
    RETURN_EXACT();

  switch (libfn) {
  case llvm::LibFunc_memset: // void* memset(void *ptr, int val, size_t bytes)
    BB.addInstr(make_unique<Memset>(*args[0], *args[1], *args[2], 1));
    RETURN_VAL(make_unique<UnaryOp>(*ty, value_name(i), *args[0],
                                    UnaryOp::Copy));
  case llvm::LibFunc_strlen:
    if (auto BI = llvm::dyn_cast<llvm::BitCastInst>(i.getArgOperand(0))) {
      auto G = llvm::dyn_cast<llvm::GlobalVariable>(BI->getOperand(0));
      if (G && G->isConstant() && G->hasDefinitiveInitializer()) {
        auto C = llvm::dyn_cast<llvm::ConstantDataArray>(G->getInitializer());
        if (C && C->isCString()) {
          RETURN_VAL(make_unique<UnaryOp>(*ty, value_name(i),
              *make_intconst(C->getAsCString().size(), ty->bits()),
              UnaryOp::Copy));
        }
      }
    }
    RETURN_VAL(make_unique<Strlen>(*ty, value_name(i), *args[0]));
  case llvm::LibFunc_memcmp:
  case llvm::LibFunc_bcmp: {
    RETURN_VAL(
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
      RETURN_VAL(unique_ptr<UnaryOp>(Op));

    BB.addInstr(unique_ptr<UnaryOp>(Op));
    RETURN_VAL(
      make_unique<ConversionOp>(*ty, value_name(i), *Op, ConversionOp::Trunc));
  }

  case llvm::LibFunc_abs:
  case llvm::LibFunc_labs:
  case llvm::LibFunc_llabs:
    RETURN_VAL(make_unique<BinOp>(*ty, value_name(i), *args[0],
                                  *make_intconst(1, 1), BinOp::Abs));

  case llvm::LibFunc_fabs:
  case llvm::LibFunc_fabsf:
    RETURN_VAL(make_unique<UnaryOp>(*ty, value_name(i), *args[0], UnaryOp::FAbs,
                                    parse_fmath(i)));

  case llvm::LibFunc_ceil:
  case llvm::LibFunc_ceilf:
    RETURN_VAL(
      make_unique<UnaryOp>(*ty, value_name(i), *args[0], UnaryOp::Ceil));

  case llvm::LibFunc_floor:
  case llvm::LibFunc_floorf:
    RETURN_VAL(
      make_unique<UnaryOp>(*ty, value_name(i), *args[0], UnaryOp::Floor));

  case llvm::LibFunc_round:
  case llvm::LibFunc_roundf:
    RETURN_VAL(
      make_unique<UnaryOp>(*ty, value_name(i), *args[0], UnaryOp::Round));

  case llvm::LibFunc_roundeven:
  case llvm::LibFunc_roundevenf:
    RETURN_VAL(
      make_unique<UnaryOp>(*ty, value_name(i), *args[0], UnaryOp::RoundEven));

  case llvm::LibFunc_trunc:
  case llvm::LibFunc_truncf:
    RETURN_VAL(
      make_unique<UnaryOp>(*ty, value_name(i), *args[0], UnaryOp::Trunc));

  case llvm::LibFunc_sqrt:
  case llvm::LibFunc_sqrtf:
    BB.addInstr(make_unique<Assume>(*args[0], Assume::WellDefined));
    RETURN_VAL(
      make_unique<UnaryOp>(*ty, value_name(i), *args[0], UnaryOp::Sqrt));

  case llvm::LibFunc_fwrite: {
    auto size = getInt(*args[1]);
    auto count = getInt(*args[2]);
    if (size && count) {
      auto bytes = *size * *count;
      // size_t fwrite(const void *ptr, 0, 0, FILE *stream) -> 0
      if (bytes == 0)
        RETURN_VAL(
          make_unique<UnaryOp>(*ty, value_name(i),
                               *make_intconst(0, ty->bits()), UnaryOp::Copy));

      // (void)fwrite(const void *ptr, 1, 1, FILE *stream) ->
      //   (void)fputc(int c, FILE *stream))
      if (bytes == 1 && i.use_empty() && TLI.has(llvm::LibFunc_fputc)) {
        fputc_attr();
        auto &byteTy = get_int_type(8); // FIXME
        auto &i32 = get_int_type(32);
        auto call
          = make_unique<FnCall>(i32, value_name(i), "@fputc", move(attrs));
        auto load
          = make_unique<Load>(byteTy, value_name(i) + "#load", *args[0], 1);
        auto load_zext
           = make_unique<ConversionOp>(i32, value_name(i) + "#zext", *load,
                                       ConversionOp::ZExt);
        call->addArg(*load_zext, move(param_attrs[0]));
        call->addArg(*args[3], move(param_attrs[1]));
        BB.addInstr(move(load));
        BB.addInstr(move(load_zext));
        RETURN_VAL(move(call));
      }
    }
  }
  [[fallthrough]];
  case llvm::LibFunc_fwrite_unlocked:
  case llvm::LibFunc_fread:
  case llvm::LibFunc_fread_unlocked:
    ret_and_args_no_undef();
    attrs.set(FnAttrs::NoThrow);
    attrs.set(FnAttrs::NoFree);
    set_param(0, ParamAttrs::NoCapture);
    set_param(3, ParamAttrs::NoCapture);
    RETURN_EXACT();

  case llvm::LibFunc_fopen:
    ret_and_args_no_undef();
    attrs.set(FnAttrs::NoThrow);
    attrs.set(FnAttrs::NoFree);
    attrs.set(FnAttrs::NoAlias);
    set_param(0, ParamAttrs::NoCapture);
    set_param(0, ParamAttrs::ReadOnly);
    set_param(1, ParamAttrs::NoCapture);
    set_param(1, ParamAttrs::ReadOnly);
    RETURN_EXACT();

  case llvm::LibFunc_fdopen:
    ret_and_args_no_undef();
    attrs.set(FnAttrs::NoThrow);
    attrs.set(FnAttrs::NoFree);
    attrs.set(FnAttrs::NoAlias);
    set_param(1, ParamAttrs::NoCapture);
    set_param(1, ParamAttrs::ReadOnly);
    RETURN_EXACT();

  case llvm::LibFunc_fputc:
  case llvm::LibFunc_fputc_unlocked:
  case llvm::LibFunc_fstat:
    fputc_attr();
    RETURN_EXACT();

  case llvm::LibFunc_fputs_unlocked:
    ret_and_args_no_undef();
    attrs.set(FnAttrs::NoThrow);
    attrs.set(FnAttrs::NoFree);
    set_param(0, ParamAttrs::ReadOnly);
    set_param(0, ParamAttrs::NoCapture);
    set_param(1, ParamAttrs::NoCapture);
    RETURN_EXACT();

  case llvm::LibFunc_clearerr:
  case llvm::LibFunc_closedir:
  case llvm::LibFunc_fseek:
  case llvm::LibFunc_ftell:
  case llvm::LibFunc_fgetc:
  case llvm::LibFunc_fgetc_unlocked:
  case llvm::LibFunc_fseeko:
  case llvm::LibFunc_ftello:
  case llvm::LibFunc_fileno:
  case llvm::LibFunc_fflush:
  case llvm::LibFunc_fclose:
  case llvm::LibFunc_fsetpos:
  case llvm::LibFunc_ftrylockfile:
    ret_and_args_no_undef();
    attrs.set(FnAttrs::NoThrow);
    set_param(0, ParamAttrs::NoCapture);
    RETURN_EXACT();

  case llvm::LibFunc_fgets:
  case llvm::LibFunc_fgets_unlocked:
    ret_and_args_no_undef();
    attrs.set(FnAttrs::NoThrow);
    attrs.set(FnAttrs::NoFree);
    set_param(0, ParamAttrs::NoCapture);
    set_param(2, ParamAttrs::NoCapture);
    RETURN_EXACT();

  case llvm::LibFunc_open:
  case llvm::LibFunc_open64:
    ret_and_args_no_undef();
    attrs.set(FnAttrs::NoFree);
    set_param(0, ParamAttrs::NoCapture);
    set_param(0, ParamAttrs::ReadOnly);
    RETURN_EXACT();

  case llvm::LibFunc_read:
    ret_and_args_no_undef();
    attrs.set(FnAttrs::NoFree);
    set_param(1, ParamAttrs::NoCapture);
    RETURN_EXACT();

  case llvm::LibFunc_write:
    ret_and_args_no_undef();
    attrs.set(FnAttrs::NoFree);
    set_param(1, ParamAttrs::NoCapture);
    set_param(1, ParamAttrs::ReadOnly);
    RETURN_EXACT();

  case llvm::LibFunc_gets:
  case llvm::LibFunc_getchar:
  case llvm::LibFunc_getchar_unlocked:
  case llvm::LibFunc_putchar:
  case llvm::LibFunc_putchar_unlocked:
    ret_and_args_no_undef();
    attrs.set(FnAttrs::NoThrow);
    attrs.set(FnAttrs::NoFree);
    RETURN_EXACT();

  case llvm::LibFunc_stat:
  case llvm::LibFunc_lstat:
    ret_and_args_no_undef();
    attrs.set(FnAttrs::NoThrow);
    attrs.set(FnAttrs::NoFree);
    set_param(0, ParamAttrs::NoCapture);
    set_param(0, ParamAttrs::ReadOnly);
    set_param(1, ParamAttrs::NoCapture);
    RETURN_EXACT();

  case llvm::LibFunc_access:
  case llvm::LibFunc_chmod:
  case llvm::LibFunc_chown:
  case llvm::LibFunc_getpwnam:
  case llvm::LibFunc_mkdir:
  case llvm::LibFunc_perror:
  case llvm::LibFunc_rmdir:
  case llvm::LibFunc_remove:
  case llvm::LibFunc_realpath:
  case llvm::LibFunc_unlink:
    ret_and_args_no_undef();
    attrs.set(FnAttrs::NoThrow);
    attrs.set(FnAttrs::NoFree);
    set_param(0, ParamAttrs::NoCapture);
    set_param(0, ParamAttrs::ReadOnly);
    RETURN_EXACT();

  case llvm::LibFunc_opendir:
    ret_and_args_no_undef();
    attrs.set(FnAttrs::NoThrow);
    attrs.set(FnAttrs::NoFree);
    attrs.set(FnAttrs::NoAlias);
    set_param(0, ParamAttrs::NoCapture);
    set_param(0, ParamAttrs::ReadOnly);
    RETURN_EXACT();

  case llvm::LibFunc_rename:
  case llvm::LibFunc_utime:
  case llvm::LibFunc_utimes:
  case llvm::LibFunc_dunder_isoc99_sscanf:
    ret_and_args_no_undef();
    attrs.set(FnAttrs::NoThrow);
    attrs.set(FnAttrs::NoFree);
    set_param(0, ParamAttrs::NoCapture);
    set_param(0, ParamAttrs::ReadOnly);
    set_param(1, ParamAttrs::NoCapture);
    set_param(1, ParamAttrs::ReadOnly);
    RETURN_EXACT();

  case llvm::LibFunc_getc:
  case llvm::LibFunc_getc_unlocked:
  case llvm::LibFunc_getlogin_r:
  case llvm::LibFunc_mktime:
  case llvm::LibFunc_rewind:
  case llvm::LibFunc_uname:
    ret_and_args_no_undef();
    attrs.set(FnAttrs::NoThrow);
    attrs.set(FnAttrs::NoFree);
    set_param(0, ParamAttrs::NoCapture);
    RETURN_EXACT();

  case llvm::LibFunc_gettimeofday:
    ret_and_args_no_undef();
    attrs.set(FnAttrs::NoThrow);
    attrs.set(FnAttrs::NoFree);
    set_param(0, ParamAttrs::NoCapture);
    set_param(1, ParamAttrs::NoCapture);
    RETURN_EXACT();

  case llvm::LibFunc_atof:
  case llvm::LibFunc_ferror:
  case llvm::LibFunc_getenv:
    ret_and_args_no_undef();
    attrs.set(FnAttrs::NoThrow);
    attrs.set(FnAttrs::NoWrite);
    attrs.set(FnAttrs::NoFree);
    set_param(0, ParamAttrs::NoCapture);
    RETURN_EXACT();

  case llvm::LibFunc_frexp:
  case llvm::LibFunc_frexpf:
  case llvm::LibFunc_frexpl:
  case llvm::LibFunc_putc:
  case llvm::LibFunc_putc_unlocked:
  case llvm::LibFunc_ungetc:
    ret_and_args_no_undef();
    attrs.set(FnAttrs::NoThrow);
    attrs.set(FnAttrs::NoFree);
    set_param(1, ParamAttrs::NoCapture);
    RETURN_EXACT();

  case llvm::LibFunc_ldexp:
  case llvm::LibFunc_ldexpf:
  case llvm::LibFunc_ldexpl:
    ret_and_args_no_undef();
    attrs.set(FnAttrs::NoThrow);
    attrs.set(FnAttrs::NoFree);
    attrs.set(FnAttrs::NoRead);
    RETURN_EXACT();

  default:
    RETURN_APPROX();
  }
}

}
