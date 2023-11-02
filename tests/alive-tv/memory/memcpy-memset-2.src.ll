declare void @llvm.memcpy.p0i8.p0i8.i32(ptr, ptr, i32, i1)
declare void @llvm.memset.p0i8.i8(ptr, i8, i32, i1)

define i8 @f() {
  %p1 = alloca [16 x i8]
  %p3 = alloca [16 x i8]
  call void @llvm.memset.p0i8.i8(ptr %p1, i8 3, i32 16, i1 0)
  call void @llvm.memcpy.p0i8.p0i8.i32(ptr %p3, ptr %p1, i32 16, i1 0)
  %p5 = getelementptr i8, ptr %p3 , i32 6
  %v = load i8, ptr %p5
  ret i8 %v
}
