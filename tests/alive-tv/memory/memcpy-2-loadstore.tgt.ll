define void @fn(i8* %p, i8* %q) {
  %p2 = bitcast i8* %p to i32*
  %q2 = bitcast i8* %q to i32*
  %v = load i32, i32* %p2
  store i32 %v, i32* %q2
  ret void
}

declare void @llvm.memcpy.p0i8.p0i8.i64(i8*, i8*, i64, i1)

