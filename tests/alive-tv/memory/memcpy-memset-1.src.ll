declare void @llvm.memcpy.p0i8.p0i8.i32(ptr, ptr, i32, i1)
declare void @llvm.memset.p0i8.i8(ptr, i8, i32, i1)

define i8 @f() {
  %p1 = alloca [16 x i8]
  call void @llvm.memset.p0i8.i8(ptr %p1, i8 3, i32 2, i1 0)
  %p3 = getelementptr i8, ptr %p1, i32 2
  call void @llvm.memcpy.p0i8.p0i8.i32(ptr %p3, ptr %p1, i32 3, i1 0)
  %v = load i8, ptr %p3
  ret i8 %v
}
