declare void @llvm.lifetime.start.p0i8(i64, ptr)
declare void @llvm.lifetime.end.p0i8(i64, ptr)
declare ptr @malloc(i64)

define i8 @src() {
  %p = call ptr @malloc(i64 1)
  store i8 1, ptr %p
  call void @llvm.lifetime.start.p0i8(i64 1, ptr %p)
  %v = load i8, ptr %p
  call void @llvm.lifetime.end.p0i8(i64 1, ptr %p)
  ret i8 %v
}

define i8 @tgt() {
  ret i8 poison
}
