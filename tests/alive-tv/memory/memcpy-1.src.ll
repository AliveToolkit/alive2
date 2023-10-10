declare void @llvm.memcpy.p0i8.p0i8.i32(ptr, ptr, i32, i1)

define i8 @f() {
  %p1 = alloca [16 x i8]
  %p3 = alloca [16 x i8]
  store i8 3, ptr %p1
  call void @llvm.memcpy.p0i8.p0i8.i32(ptr %p3, ptr %p1, i32 1, i1 0)
  %v = load i8, ptr %p3
  ret i8 %v
}
