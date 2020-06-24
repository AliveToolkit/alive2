; Relevant test: Transforms/InstCombine/lifetime-sanitizer.ll
declare void @llvm.lifetime.start.p0i8(i64, i8*)
declare void @llvm.lifetime.end.p0i8(i64, i8*)

declare void @f(i8*)

define void @src() {
  %p0 = alloca i8
  call void @llvm.lifetime.start.p0i8(i64 1, i8* %p0)
  call void @llvm.lifetime.end.p0i8(i64 1, i8* %p0)
  call void @f(i8* %p0)
  ret void
}

define void @tgt() {
  %p0 = alloca i8
  call void @f(i8* %p0)
  ret void
}
