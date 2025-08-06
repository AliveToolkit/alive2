declare void @llvm.lifetime.start.p0i8(i64, ptr)
declare void @llvm.lifetime.end.p0i8(i64, ptr)

define void @src() {
  ret void
}

define void @tgt() {
  ; lifetime with a poison ptr is a nop
  call void @llvm.lifetime.start.p0i8(i64 1, ptr poison)
  call void @llvm.lifetime.end.p0i8(i64 1, ptr poison)
  ret void
}
