; ERROR: Mismatch in memory

define void @fn(i8* %p, i8* %q) {
  tail call void @llvm.memcpy.p0i8.p0i8.i64(i8* align 4 %q, i8* align 4 %p, i64 4, i1 false)
  ret void
}

declare void @llvm.memcpy.p0i8.p0i8.i64(i8*, i8*, i64, i1)
