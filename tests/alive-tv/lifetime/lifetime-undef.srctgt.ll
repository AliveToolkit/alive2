declare void @llvm.lifetime.start.p0i8(i64, i8*)
declare void @llvm.lifetime.end.p0i8(i64, i8*)

define void @src() {
  ret void
}

define void @tgt() {
  ; lifetime undef is nop
  call void @llvm.lifetime.start.p0i8(i64 1, i8* undef)
  call void @llvm.lifetime.end.p0i8(i64 1, i8* undef)
  ret void
}
