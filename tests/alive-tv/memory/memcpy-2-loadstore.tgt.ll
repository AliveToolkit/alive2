define void @fn(ptr %p, ptr %q) {
  %v = load i32, ptr %p
  store i32 %v, ptr %q
  ret void
}

declare void @llvm.memcpy.p0i8.p0i8.i64(ptr, ptr, i64, i1)
