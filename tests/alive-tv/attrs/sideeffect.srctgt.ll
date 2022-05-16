declare void @llvm.sideeffect()

define void @src() {
  call void @llvm.sideeffect() readnone
  ret void
}

define void @tgt() {
  ret void
}
