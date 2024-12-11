define void @src(ptr %src, ptr %dst) {
  %load.src = load i32, ptr %src
  store i32 %load.src, ptr %dst
  ret void
}

define void @tgt(ptr %src, ptr %dst) {
  call void @llvm.memmove.p0.p0.i64(ptr %dst, ptr %src, i64 4, i1 false)
  ret void
}
