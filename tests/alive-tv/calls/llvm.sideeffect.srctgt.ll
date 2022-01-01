; ERROR: Mismatch in memory

define i4 @src(i4* %p) {
  call void @llvm.sideeffect()
  ret i4 6
}

define i4 @tgt(i4* %p) {
  ret i4 6
}

declare void @llvm.sideeffect()
