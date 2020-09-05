; ERROR: Value mismatch

declare void @llvm.memcpy.p0i8.p0i8.i32(i8*, i8*, i32, i1)

define i8 @src() {
  %p1 = alloca i8
  %p2 = alloca i8
  store i8 42, i8* %p1
  call void @llvm.memcpy.p0i8.p0i8.i32(i8* %p1, i8* %p1, i32 1, i1 0)
  call void @llvm.memcpy.p0i8.p0i8.i32(i8* %p2, i8* %p1, i32 1, i1 0)
  %v = load i8, i8* %p2
  ret i8 %v
}

define i8 @tgt() {
  ret i8 43
}
