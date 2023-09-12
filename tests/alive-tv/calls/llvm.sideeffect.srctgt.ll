; ERROR: Mismatch in memory

define i4 @src(ptr %p) {
  call void @llvm.sideeffect()
  ret i4 6
}

define i4 @tgt(ptr %p) {
  ret i4 6
}

declare void @llvm.sideeffect()
