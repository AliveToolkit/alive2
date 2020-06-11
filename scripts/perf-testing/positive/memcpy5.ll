declare void @llvm.memcpy.p0i8.p0i8.i32(i8*, i8*, i32, i1)
declare i32 @memcmp(i8* nocapture, i8* nocapture, i64)

@a = external global [X x i8]
@b = external global [X x i8]

define i32 @src() {
  %p1 = bitcast [X x i8]* @a to i8*
  %p2 = bitcast [X x i8]* @b to i8*
  call void @llvm.memcpy.p0i8.p0i8.i32(i8* %p1, i8* %p2, i32 X, i1 0)
  %r = call i32 @memcmp(i8* %p1, i8* %p2, i64 X)
  ret i32 %r
}

define i32 @tgt() {
  %p1 = bitcast [X x i8]* @a to i8*
  %p2 = bitcast [X x i8]* @b to i8*
  call void @llvm.memcpy.p0i8.p0i8.i32(i8* %p1, i8* %p2, i32 X, i1 0)
  ret i32 0
}

