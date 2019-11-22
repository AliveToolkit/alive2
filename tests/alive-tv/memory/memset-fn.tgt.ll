; TEST-ARGS: -smt-to=5000
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"

define i32* @f(i32* returned %ptr, i32 %val, i64 %len) {
  %p = bitcast i32* %ptr to i8*
  %1 = trunc i32 %val to i8
  tail call void @llvm.memset.p0i8.i64(i8* align 1 %p, i8 %1, i64 %len, i1 false)
  ret i32* %ptr
}

declare void @llvm.memset.p0i8.i64(i8* nocapture writeonly, i8, i64, i1 immarg) #0

attributes #0 = { argmemonly nounwind willreturn }
