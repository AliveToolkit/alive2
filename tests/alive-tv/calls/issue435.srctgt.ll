target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

define void @src(i64 %i) {
entry:
  %storage = alloca [16 x i8], align 16
  %0 = getelementptr inbounds [16 x i8], [16 x i8]* %storage, i64 0, i64 0
  call void @llvm.memset.p0i8.i64(i8* noundef nonnull align 16 dereferenceable(16) %0, i8 0, i64 16, i1 false)
  %arrayidx = getelementptr inbounds [16 x i8], [16 x i8]* %storage, i64 0, i64 %i
  call void @use(i8* nonnull align 1 dereferenceable(1) %arrayidx)
  ret void
}

declare void @llvm.memset.p0i8.i64(i8*, i8, i64, i1)

declare void @use(i8* nonnull align 1 dereferenceable(1))

define void @tgt(i64 %i) {
entry:
  %storage = alloca [16 x i8], align 16
  %0 = getelementptr inbounds [16 x i8], [16 x i8]* %storage, i64 0, i64 0
  %arrayidx = getelementptr inbounds [16 x i8], [16 x i8]* %storage, i64 0, i64 %i
  call void @use(i8* nonnull align 1 dereferenceable(1) %arrayidx)
  ret void
}

; ERROR: Source is more defined than target
