declare void @llvm.memset.p0i8.i8(i8*, i8, i32, i1)

@x = global i32 0

define i32 @f() {
  %p = bitcast i32* @x to i8*
  call void @llvm.memset.p0i8.i8(i8* %p, i8 1, i32 4, i1 0)
  %v = load i32, i32* @x
  ret i32 %v
}

define i32 @f2() {
  %p = bitcast i32* @x to i8*
  call void @llvm.memset.p0i8.i8(i8* align 4 %p, i8 1, i32 4, i1 0)
  %v = load i32, i32* @x
  ret i32 %v
}
