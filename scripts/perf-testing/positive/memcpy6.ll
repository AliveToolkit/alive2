declare void @llvm.memcpy.p0i8.p0i8.i32(i8*, i8*, i32, i1)
declare i32 @memcmp(i8* nocapture, i8* nocapture, i64)

define i32 @src(i8*, i8*) {
  call void @llvm.memcpy.p0i8.p0i8.i32(i8* %0, i8* %1, i32 X, i1 0)
  %r = call i32 @memcmp(i8* %1, i8* %0, i64 X)
  ret i32 %r
}

define i32 @tgt(i8*, i8*) {
  call void @llvm.memcpy.p0i8.p0i8.i32(i8* %0, i8* %1, i32 X, i1 0)
  ret i32 0
}

