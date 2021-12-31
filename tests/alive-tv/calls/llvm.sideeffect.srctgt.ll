; ERROR: Source and target don't have the same return domain

define i4 @src(i4* %p) {
  call void @llvm.sideeffect()
  ret i4 6
}

define i4 @tgt(i4* %p) {
  ret i4 6
}

declare void @llvm.sideeffect()
