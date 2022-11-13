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

#define RETURN_EXACT()  return false
#define RETURN_APPROX() return true

static bool implict_attrs_(llvm::LibFunc libfn, FnAttrs &attrs,
                           vector<ParamAttrs> &param_attrs, bool is_void,
                           const vector<Value*> &args) {
  auto set_param = [&](unsigned i, ParamAttrs::Attribute attr) {
    if (param_attrs.size() <= i)
      param_attrs.resize(i+1);
    param_attrs[i].set(attr);
  };

  auto set_param_deref = [&](unsigned i, unsigned bytes) {
    set_param(i, ParamAttrs::Dereferenceable);
    param_attrs[i].derefBytes = bytes;
  };

  auto ret_and_args_no_undef = [&]() {
    if (!is_void)
      attrs.set(FnAttrs::NoUndef);
    for (unsigned i = 0, e = args.size(); i < e; ++i) {
      set_param(i, ParamAttrs::NoUndef);
    }
  };

  auto set_align = [&](uint64_t align) {
    attrs.align = max(attrs.align, align);
  };

  auto alloc_fns = [&](unsigned idx1, unsigned idx2 = -1u) {
    ret_and_args_no_undef();
    attrs.mem.setCanOnlyAccess(MemoryAccess::Inaccessible);
    attrs.set(FnAttrs::NoAlias);
    attrs.set(FnAttrs::WillReturn);
    attrs.set(FnAttrs::AllocSize);
    attrs.allocsize_0 = idx1;
    attrs.allocsize_1 = idx2;
    attrs.derefOrNullBytes = getIntOr(*args[idx1], 0);
    if (idx2 != -1u)
      attrs.derefOrNullBytes *= getIntOr(*args[idx2], 0);
    if (attrs.derefOrNullBytes > 0)
      attrs.set(FnAttrs::DereferenceableOrNull);
  };

  switch (libfn) {
  case llvm::LibFunc_valloc:
    set_align(4096); // page size
    [[fallthrough]];
  case llvm::LibFunc_malloc:
    alloc_fns(0);
    attrs.set(FnAttrs::NoThrow);
    attrs.set(FnAttrs::NoFree);
    attrs.allocfamily = "malloc";
    attrs.add(AllocKind::Alloc);
    attrs.add(AllocKind::Uninitialized);
    RETURN_EXACT();

  case llvm::LibFunc_aligned_alloc:
  case llvm::LibFunc_memalign:
    alloc_fns(1);
    attrs.set(FnAttrs::NoThrow);
    attrs.set(FnAttrs::NoFree);
    attrs.allocfamily = "malloc";
    attrs.add(AllocKind::Alloc);
    attrs.add(AllocKind::Uninitialized);
    set_param(0, ParamAttrs::AllocAlign);
    RETURN_EXACT();

  case llvm::LibFunc_calloc:
    alloc_fns(0, 1);
    attrs.set(FnAttrs::NoThrow);
    attrs.set(FnAttrs::NoFree);
    attrs.allocfamily = "malloc";
    attrs.add(AllocKind::Alloc);
    attrs.add(AllocKind::Zeroed);
    RETURN_EXACT();

  case llvm::LibFunc_realloc:
  case llvm::LibFunc_reallocf:
    alloc_fns(1);
    attrs.set(FnAttrs::NoThrow);
    attrs.allocfamily = "malloc";
    attrs.add(AllocKind::Realloc);
    attrs.add(AllocKind::Uninitialized);
    set_param(0, ParamAttrs::AllocPtr);
    set_param(0, ParamAttrs::NoCapture);
    RETURN_EXACT();

  case llvm::LibFunc_free:
    ret_and_args_no_undef();
    attrs.mem.setCanOnlyAccess(MemoryAccess::Inaccessible);
    attrs.set(FnAttrs::NoThrow);
    attrs.set(FnAttrs::WillReturn);
    attrs.allocfamily = "malloc";
    attrs.add(AllocKind::Free);
    set_param(0, ParamAttrs::AllocPtr);
    RETURN_EXACT();

  case llvm::LibFunc_Znwj: // new(unsigned int)
  case llvm::LibFunc_Znwm: // new(unsigned long)
    alloc_fns(0);
    attrs.set(FnAttrs::NoFree);
    attrs.allocfamily = "_Znwm";
    attrs.add(AllocKind::Alloc);
    attrs.add(AllocKind::Uninitialized);
    RETURN_EXACT();

  case llvm::LibFunc_vec_malloc:
    alloc_fns(0);
    attrs.set(FnAttrs::NoThrow);
    attrs.set(FnAttrs::NoFree);
    attrs.allocfamily = "vecmalloc";
    attrs.add(AllocKind::Alloc);
    attrs.add(AllocKind::Uninitialized);
    set_align(16);
    RETURN_EXACT();

  case llvm::LibFunc_vec_calloc:
    alloc_fns(0, 1);
    attrs.set(FnAttrs::NoThrow);
    attrs.set(FnAttrs::NoFree);
    attrs.allocfamily = "vecmalloc";
    attrs.add(AllocKind::Alloc);
    attrs.add(AllocKind::Zeroed);
    set_align(16);
    RETURN_EXACT();

  case llvm::LibFunc_vec_realloc:
    alloc_fns(1);
    attrs.set(FnAttrs::NoThrow);
    attrs.allocfamily = "vecmalloc";
    attrs.add(AllocKind::Realloc);
    attrs.add(AllocKind::Uninitialized);
    set_param(0, ParamAttrs::AllocPtr);
    set_param(0, ParamAttrs::NoCapture);
    set_align(16);
    RETURN_EXACT();

  case llvm::LibFunc_vec_free:
    ret_and_args_no_undef();
    attrs.mem.setCanOnlyAccess(MemoryAccess::Inaccessible);
    attrs.set(FnAttrs::NoThrow);
    attrs.set(FnAttrs::WillReturn);
    attrs.allocfamily = "vecmalloc";
    attrs.add(AllocKind::Free);
    set_param(0, ParamAttrs::AllocPtr);
    RETURN_EXACT();

  case llvm::LibFunc_fwrite:
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
    set_param(0, ParamAttrs::NoWrite);
    set_param(1, ParamAttrs::NoCapture);
    set_param(1, ParamAttrs::NoWrite);
    RETURN_EXACT();

  case llvm::LibFunc_fdopen:
    ret_and_args_no_undef();
    attrs.set(FnAttrs::NoThrow);
    attrs.set(FnAttrs::NoFree);
    attrs.set(FnAttrs::NoAlias);
    set_param(1, ParamAttrs::NoCapture);
    set_param(1, ParamAttrs::NoWrite);
    RETURN_EXACT();

  case llvm::LibFunc_fputc:
  case llvm::LibFunc_fputc_unlocked:
  case llvm::LibFunc_fstat:
    ret_and_args_no_undef();
    attrs.set(FnAttrs::NoThrow);
    attrs.set(FnAttrs::NoFree);
    set_param(1, ParamAttrs::NoCapture);
    RETURN_EXACT();

  case llvm::LibFunc_fputs_unlocked:
    ret_and_args_no_undef();
    attrs.set(FnAttrs::NoThrow);
    attrs.set(FnAttrs::NoFree);
    set_param(0, ParamAttrs::NoWrite);
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
    set_param(0, ParamAttrs::NoWrite);
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
    set_param(1, ParamAttrs::NoWrite);
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
    set_param(0, ParamAttrs::NoWrite);
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
    set_param(0, ParamAttrs::NoWrite);
    set_param_deref(0, 1);
    RETURN_EXACT();

  case llvm::LibFunc_opendir:
    ret_and_args_no_undef();
    attrs.set(FnAttrs::NoThrow);
    attrs.set(FnAttrs::NoFree);
    attrs.set(FnAttrs::NoAlias);
    set_param(0, ParamAttrs::NoCapture);
    set_param(0, ParamAttrs::NoWrite);
    RETURN_EXACT();

  case llvm::LibFunc_rename:
  case llvm::LibFunc_utime:
  case llvm::LibFunc_utimes:
  case llvm::LibFunc_dunder_isoc99_sscanf:
    ret_and_args_no_undef();
    attrs.set(FnAttrs::NoThrow);
    attrs.set(FnAttrs::NoFree);
    set_param(0, ParamAttrs::NoCapture);
    set_param(0, ParamAttrs::NoWrite);
    set_param(1, ParamAttrs::NoCapture);
    set_param(1, ParamAttrs::NoWrite);
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

  case llvm::LibFunc_ferror:
  case llvm::LibFunc_getenv:
    ret_and_args_no_undef();
    attrs.set(FnAttrs::NoThrow);
    attrs.set(FnAttrs::NoFree);
    attrs.mem.setCanOnlyRead();
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
    attrs.mem.setCanOnlyWrite(MemoryAccess::Errno);
    attrs.set(FnAttrs::WillReturn);
    RETURN_EXACT();

  case llvm::LibFunc_strcoll:
  case llvm::LibFunc_strcasecmp:
    set_param_deref(0, 1);
    set_param_deref(1, 1);
    [[fallthrough]];
  case llvm::LibFunc_strncasecmp:
    ret_and_args_no_undef();
    attrs.set(FnAttrs::NoThrow);
    attrs.set(FnAttrs::NoFree);
    attrs.mem.setCanOnlyRead(MemoryAccess::Args);
    attrs.set(FnAttrs::WillReturn);
    set_param(0, ParamAttrs::NoCapture);
    set_param(1, ParamAttrs::NoCapture);
    RETURN_EXACT();

  case llvm::LibFunc_printf:
  case llvm::LibFunc_puts:
    ret_and_args_no_undef();
    attrs.set(FnAttrs::NoThrow);
    attrs.set(FnAttrs::NoFree);
    set_param(0, ParamAttrs::NoCapture);
    set_param(0, ParamAttrs::NoWrite);
    set_param_deref(0, 1);
    RETURN_APPROX();

  case llvm::LibFunc_fscanf:
  case llvm::LibFunc_fprintf:
    ret_and_args_no_undef();
    attrs.set(FnAttrs::NoThrow);
    attrs.set(FnAttrs::NoFree);
    set_param(0, ParamAttrs::NoCapture);
    set_param(1, ParamAttrs::NoCapture);
    set_param(1, ParamAttrs::NoWrite);
    set_param_deref(1, 1);
    RETURN_APPROX();

  case llvm::LibFunc_sprintf:
    ret_and_args_no_undef();
    attrs.set(FnAttrs::NoThrow);
    attrs.set(FnAttrs::NoFree);
    set_param(0, ParamAttrs::NoCapture);
    set_param(1, ParamAttrs::NoCapture);
    set_param(0, ParamAttrs::NoRead);
    set_param(1, ParamAttrs::NoWrite);
    set_param_deref(0, 1);
    set_param_deref(1, 1);
    RETURN_APPROX();

  case llvm::LibFunc_strcat:
  case llvm::LibFunc_strcpy:
    set_param_deref(0, 1);
    set_param_deref(1, 1);
    [[fallthrough]];
  case llvm::LibFunc_strncat:
  case llvm::LibFunc_strncpy:
    ret_and_args_no_undef();
    attrs.mem.setCanOnlyAccess(MemoryAccess::Args);
    attrs.set(FnAttrs::NoThrow);
    attrs.set(FnAttrs::NoFree);
    attrs.set(FnAttrs::WillReturn);
    set_param(0, ParamAttrs::Returned);
    set_param(0, ParamAttrs::NoRead);
    set_param(1, ParamAttrs::NoWrite);
    set_param(0, ParamAttrs::NoCapture);
    set_param(1, ParamAttrs::NoCapture);
    set_param(0, ParamAttrs::NoAlias);
    set_param(1, ParamAttrs::NoAlias);
    RETURN_APPROX();

  case llvm::LibFunc_strcmp:
    set_param_deref(0, 1);
    set_param_deref(1, 1);
    [[fallthrough]];
  case llvm::LibFunc_strncmp:
    ret_and_args_no_undef();
    attrs.mem.setCanOnlyRead(MemoryAccess::Args);
    attrs.set(FnAttrs::NoThrow);
    attrs.set(FnAttrs::NoFree);
    attrs.set(FnAttrs::WillReturn);
    set_param(0, ParamAttrs::NoCapture);
    set_param(1, ParamAttrs::NoCapture);
    RETURN_APPROX();

  case llvm::LibFunc_atoi:
  case llvm::LibFunc_atol:
  case llvm::LibFunc_atof:
  case llvm::LibFunc_atoll:
    set_param(0, ParamAttrs::NoCapture);
    [[fallthrough]];
  case llvm::LibFunc_strchr:
  case llvm::LibFunc_strrchr:
    set_param_deref(0, 1);
    [[fallthrough]];
  case llvm::LibFunc_memchr:
  case llvm::LibFunc_memrchr:
    ret_and_args_no_undef();
    attrs.mem.setCanOnlyRead(MemoryAccess::Args);
    attrs.set(FnAttrs::NoThrow);
    attrs.set(FnAttrs::NoFree);
    attrs.set(FnAttrs::WillReturn);
    RETURN_APPROX();

  case llvm::LibFunc_strstr:
  case llvm::LibFunc_strpbrk:
    attrs.mem.setCanOnlyRead(MemoryAccess::Args);
    attrs.set(FnAttrs::NoThrow);
    attrs.set(FnAttrs::NoFree);
    attrs.set(FnAttrs::WillReturn);
    set_param_deref(0, 1);
    set_param_deref(1, 1);
    set_param(1, ParamAttrs::NoCapture);
    RETURN_APPROX();

  default:
    RETURN_APPROX();
  }
}

namespace llvm_util {

bool llvm_implict_attrs(llvm::Function &f, const llvm::TargetLibraryInfo &TLI,
                        FnAttrs &attrs, vector<ParamAttrs> &param_attrs,
                        const vector<Value*> &args) {
  llvm::LibFunc libfn;
  if (!TLI.getLibFunc(f, libfn) || !TLI.has(libfn))
    return false;
  return implict_attrs_(libfn, attrs, param_attrs,
                        f.getReturnType()->isVoidTy(), args);
}

#undef RETURN_EXACT
#undef RETURN_APPROX

#define RETURN_VAL(op)  return { op, false }
#define RETURN_EXACT()  return { nullptr, false }
#define RETURN_APPROX() return { nullptr, true }

pair<unique_ptr<Instr>, bool>
known_call(llvm::CallInst &i, const llvm::TargetLibraryInfo &TLI,
           BasicBlock &BB, const vector<Value*> &args, FnAttrs &&attrs,
           vector<ParamAttrs> &param_attrs) {

  auto ty = llvm_type2alive(i.getType());
  if (!ty)
    RETURN_EXACT();

  auto decl = i.getCalledFunction();
  llvm::LibFunc libfn;
  if (!decl || !TLI.getLibFunc(*decl, libfn) || !TLI.has(libfn))
    RETURN_EXACT();

  switch (libfn) {
  case llvm::LibFunc_memset: // void* memset(void *ptr, int val, size_t bytes)
    BB.addInstr(make_unique<Memset>(*args[0], *args[1], *args[2], 1));
    RETURN_VAL(make_unique<UnaryOp>(*ty, value_name(i), *args[0],
                                    UnaryOp::Copy));

  // void memset_pattern4(void *ptr, void *pattern, size_t bytes)
  case llvm::LibFunc_memset_pattern4:
    RETURN_VAL(make_unique<MemsetPattern>(*args[0], *args[1], *args[2], 4));
  case llvm::LibFunc_memset_pattern8:
    RETURN_VAL(make_unique<MemsetPattern>(*args[0], *args[1], *args[2], 8));
  case llvm::LibFunc_memset_pattern16:
    RETURN_VAL(make_unique<MemsetPattern>(*args[0], *args[1], *args[2], 16));
  case llvm::LibFunc_strlen:
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
    RETURN_VAL(make_unique<FpUnaryOp>(*ty, value_name(i), *args[0],
                                      FpUnaryOp::FAbs, parse_fmath(i)));

  case llvm::LibFunc_ceil:
  case llvm::LibFunc_ceilf:
    RETURN_VAL(make_unique<FpUnaryOp>(*ty, value_name(i), *args[0],
                                      FpUnaryOp::Ceil, parse_fmath(i)));

  case llvm::LibFunc_floor:
  case llvm::LibFunc_floorf:
    RETURN_VAL(make_unique<FpUnaryOp>(*ty, value_name(i), *args[0],
                                      FpUnaryOp::Floor, parse_fmath(i)));

  case llvm::LibFunc_nearbyint:
  case llvm::LibFunc_nearbyintf:
    RETURN_VAL(make_unique<FpUnaryOp>(*ty, value_name(i), *args[0],
                                      FpUnaryOp::NearbyInt, parse_fmath(i)));

  case llvm::LibFunc_rint:
  case llvm::LibFunc_rintf:
    RETURN_VAL(make_unique<FpUnaryOp>(*ty, value_name(i), *args[0],
                                      FpUnaryOp::RInt, parse_fmath(i)));

  case llvm::LibFunc_round:
  case llvm::LibFunc_roundf:
    RETURN_VAL(make_unique<FpUnaryOp>(*ty, value_name(i), *args[0],
                                      FpUnaryOp::Round, parse_fmath(i)));

  case llvm::LibFunc_roundeven:
  case llvm::LibFunc_roundevenf:
    RETURN_VAL(make_unique<FpUnaryOp>(*ty, value_name(i), *args[0],
                                      FpUnaryOp::RoundEven, parse_fmath(i)));

  case llvm::LibFunc_trunc:
  case llvm::LibFunc_truncf:
    RETURN_VAL(make_unique<FpUnaryOp>(*ty, value_name(i), *args[0],
                                      FpUnaryOp::Trunc, parse_fmath(i)));

  case llvm::LibFunc_copysign:
  case llvm::LibFunc_copysignf:
    RETURN_VAL(make_unique<FpBinOp>(*ty, value_name(i), *args[0], *args[1],
                                    FpBinOp::CopySign, parse_fmath(i)));

  case llvm::LibFunc_sqrt:
  case llvm::LibFunc_sqrtf:
    BB.addInstr(make_unique<Assume>(*args[0], Assume::WellDefined));
    RETURN_VAL(make_unique<FpUnaryOp>(*ty, value_name(i), *args[0],
                                      FpUnaryOp::Sqrt, parse_fmath(i)));
  case llvm::LibFunc_fwrite: {
    auto size = getInt(*args[1]);
    auto count = getInt(*args[2]);
    if (size || count) {
      // size_t fwrite(const void *ptr, 0, 0, FILE *stream) -> 0
      if ((size && *size == 0) || (count && *count == 0))
        RETURN_VAL(
          make_unique<UnaryOp>(*ty, value_name(i),
                               *make_intconst(0, ty->bits()), UnaryOp::Copy));
    }
    if (size && count) {
      auto bytes = *size * *count;
      // (void)fwrite(const void *ptr, 1, 1, FILE *stream) ->
      //   (void)fputc(int c, FILE *stream))
      if (bytes == 1 && i.use_empty() && TLI.has(llvm::LibFunc_fputc)) {
        auto &byteTy = get_int_type(8); // FIXME
        auto &i32 = get_int_type(32);
        auto load
          = make_unique<Load>(byteTy, value_name(i) + "#load", *args[0], 1);
        auto load_zext
           = make_unique<ConversionOp>(i32, value_name(i) + "#zext", *load,
                                       ConversionOp::ZExt);

        ENSURE(!implict_attrs_(llvm::LibFunc_fputc, attrs, param_attrs, false,
                               {load_zext.get(), args[3]}));
        auto call
          = make_unique<FnCall>(i32, value_name(i), "@fputc", std::move(attrs));
        call->addArg(*load_zext, std::move(param_attrs[0]));
        call->addArg(*args[3], std::move(param_attrs[1]));
        BB.addInstr(std::move(load));
        BB.addInstr(std::move(load_zext));
        RETURN_VAL(std::move(call));
      }
    }
    break;
  }
  default:
    break;
  }

  return { nullptr, llvm_implict_attrs(*decl, TLI, attrs, param_attrs, args) };
}

}
