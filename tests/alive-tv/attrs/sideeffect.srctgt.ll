declare void @llvm.sideeffect()

define void @src() {
  call void @llvm.sideeffect() memory(none)
  ret void
}

define void @tgt() {
  ret void
}
