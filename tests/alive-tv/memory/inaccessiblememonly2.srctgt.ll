define i4 @src(ptr %p) {
  %v = load i4, ptr %p
  call void @fn()
  ret i4 %v
}

define i4 @tgt(ptr %p) {
  call void @fn()
  %v = load i4, ptr %p
  ret i4 %v
}

declare void @fn() memory(inaccessiblemem: readwrite)
