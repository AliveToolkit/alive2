declare void @llvm.memcpy.p0i8.p0i8.i32(i8*, i8*, i32, i1)
declare i32 @memcmp(i8* nocapture, i8* nocapture, i64)

define i32 @src(i8*) {
  %p1 = alloca [X x i8]
  %p2 = bitcast [X x i8]* %p1 to i8*
  %p3 = alloca [X x i8]
  %p4 = bitcast [X x i8]* %p3 to i8*
  call void @llvm.memcpy.p0i8.p0i8.i32(i8* %p2, i8* %0, i32 X, i1 0)
  call void @llvm.memcpy.p0i8.p0i8.i32(i8* %p4, i8* %0, i32 X, i1 0)
  %r = call i32 @memcmp(i8* %p2, i8* %p4, i64 X)
  ret i32 %r
}

define i32 @tgt(i8*) {
  ret i32 0
}

