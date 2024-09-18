target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"

define ptr @f(ptr %ptr, i32 %val, i64 %len) {
  %1 = trunc i32 %val to i8
  call void @llvm.memset.p0i8.i64(ptr align 1 %ptr, i8 %1, i64 %len, i1 false)
  ret ptr %ptr
}

declare void @llvm.memset.p0i8.i64(ptr, i8, i64, i1)
