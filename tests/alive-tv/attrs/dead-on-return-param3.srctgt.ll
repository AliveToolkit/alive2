define void @src(ptr dead_on_return(8) %0) {
  call void @llvm.memset.p0.i64(ptr %0, i8 0, i64 16, i1 false)
  ret void
}

define void @tgt(ptr dead_on_return(8) %0) {
  ret void
}

declare void @llvm.memset.p0.i64(ptr, i8, i64, i1)

; ERROR: Mismatch in memory
