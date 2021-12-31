; ERROR: Source and target don't have the same return domain

define i4 @src(i4* %p) {
  %v = load i4, i4* %p
  call void @fn()
  ret i4 %v
}

define i4 @tgt(i4* %p) {
  %v = load i4, i4* %p
  ret i4 %v
}

declare void @fn() inaccessiblememonly
