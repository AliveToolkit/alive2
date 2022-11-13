; ERROR: Source and target don't have the same return domain

define i4 @src(ptr %p) {
  %v = load i4, ptr %p
  call void @fn()
  ret i4 %v
}

define i4 @tgt(ptr %p) {
  %v = load i4, ptr %p
  ret i4 %v
}

declare void @fn() memory(inaccessiblemem: readwrite)
