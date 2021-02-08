target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

declare void @llvm.memset.p0i8.i64(i8* nocapture writeonly, i8, i64, i1 immarg)

define void @f(i8* %out) {
entry:
  call void @llvm.memset.p0i8.i64(i8* align 4 %out, i8 0, i64 0, i1 false)
  ret void
}
