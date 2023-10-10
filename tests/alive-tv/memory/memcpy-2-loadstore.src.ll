; ERROR: Mismatch in memory

define void @fn(ptr %p, ptr %q) {
  call void @llvm.memcpy.p0i8.p0i8.i64(ptr align 4 %q, ptr align 4 %p, i64 4, i1 false)
  ret void
}

declare void @llvm.memcpy.p0i8.p0i8.i64(ptr, ptr, i64, i1)
