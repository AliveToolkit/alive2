declare void @llvm.memset.p0i8.i8(ptr, i8, i32, i1)

@x = global i32 0

define i32 @f() {
  call void @llvm.memset.p0i8.i8(ptr @x, i8 1, i32 4, i1 0)
  %v = load i32, ptr @x
  ret i32 %v
}

define i32 @f2() {
  call void @llvm.memset.p0i8.i8(ptr align 4 @x, i8 1, i32 4, i1 0)
  %v = load i32, ptr @x
  ret i32 %v
}
