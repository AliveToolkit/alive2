declare void @llvm.memset.p0i8.i128(i8*, i8, i128, i1)

define void @f(i8* %p, i128 %n) {
  store i8 0, i8* %p
  %p2 = getelementptr i8, i8* %p, i64 1
  call void @llvm.memset.p0i8.i128(i8* %p2, i8 0, i128 %n, i1 0)
  ret void
}
