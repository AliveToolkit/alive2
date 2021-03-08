declare void @llvm.lifetime.start.p0i8(i64, i8*)
declare void @llvm.lifetime.end.p0i8(i64, i8*)
declare i8* @malloc(i64)

define i8 @src() {
  %p = call i8* @malloc(i64 1)
  store i8 1, i8* %p
  call void @llvm.lifetime.start.p0i8(i64 1, i8* %p)
  %v = load i8, i8* %p
  call void @llvm.lifetime.end.p0i8(i64 1, i8* %p)
  ret i8 %v
}

define i8 @tgt() {
  ret i8 poison
}
