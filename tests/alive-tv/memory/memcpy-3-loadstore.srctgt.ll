; ERROR: Mismatch in memory

define void @src(ptr %p, ptr %q) {
  call void @llvm.memcpy.p0i8.p0i8.i64(ptr %q, ptr %p, i64 1, i1 false)
  ret void
}

define void @tgt(ptr %p, ptr %q) {
  %v = load i8, ptr %p
  store i8 %v, ptr %q
  ret void
}

declare void @llvm.memcpy.p0i8.p0i8.i64(ptr, ptr, i64, i1)
