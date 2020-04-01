; ERROR: Mismatch in memory

define void @src(i8* %p, i8* %q) {
  tail call void @llvm.memcpy.p0i8.p0i8.i64(i8* align 4 %q, i8* align 4 %p, i64 1, i1 false)
  ret void
}

define void @tgt(i8* %p, i8* %q) {
  %v = load i8, i8* %p
  store i8 %v, i8* %q
  ret void
}

declare void @llvm.memcpy.p0i8.p0i8.i64(i8*, i8*, i64, i1)
