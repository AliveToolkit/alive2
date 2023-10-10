declare void @llvm.memset.p0i8.i128(ptr, i8, i128, i1)

define void @f(ptr %p, i128 %n) {
  store i8 0, ptr %p
  %p2 = getelementptr i8, ptr %p, i64 1
  call void @llvm.memset.p0i8.i128(ptr %p2, i8 0, i128 %n, i1 0)
  ret void
}
