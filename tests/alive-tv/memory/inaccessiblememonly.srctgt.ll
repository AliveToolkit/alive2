define i4 @src() memory(inaccessiblemem: readwrite) {
  call void @fn()
  ret i4 0
}

define i4 @tgt() memory(inaccessiblemem: readwrite) {
  unreachable
}

declare void @fn()
