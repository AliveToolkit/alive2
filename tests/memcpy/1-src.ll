declare void @llvm.memcpy.p0i8.p0i8.i32(i8*, i8*, i32, i1)

define i8 @f() {
  %p1 = alloca [16 x i8]
  %p2 = bitcast [16 x i8]* %p1 to i8*
  %p3 = alloca [16 x i8]
  %p4 = bitcast [16 x i8]* %p3 to i8*
  store i8 3, i8* %p2
  call void @llvm.memcpy.p0i8.p0i8.i32(i8* %p4, i8* %p2, i32 1, i1 0)
  %v = load i8, i8* %p4
  ret i8 %v
}
