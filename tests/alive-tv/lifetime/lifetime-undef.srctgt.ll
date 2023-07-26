declare void @llvm.lifetime.start.p0i8(i64, ptr)
declare void @llvm.lifetime.end.p0i8(i64, ptr)

define void @src() {
  ret void
}

define void @tgt() {
  ; lifetime undef is nop
  call void @llvm.lifetime.start.p0i8(i64 1, ptr undef)
  call void @llvm.lifetime.end.p0i8(i64 1, ptr undef)
  ret void
}
