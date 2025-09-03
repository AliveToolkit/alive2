define void @src(ptr %0, ptr dead_on_return %1) {
  call void @llvm.memset.p0.i64(ptr %0, i8 0, i64 1024, i1 false)
  call void @llvm.memset.p0.i64(ptr %1, i8 0, i64 1024, i1 false)
  ret void
}

define void @tgt(ptr %0, ptr dead_on_return %1) {
  call void @llvm.memset.p0.i64(ptr %0, i8 0, i64 1024, i1 false)
  ret void
}

declare void @llvm.memset.p0.i64(ptr, i8, i64, i1)
