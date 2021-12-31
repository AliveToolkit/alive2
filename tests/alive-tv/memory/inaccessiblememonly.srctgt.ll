define i4 @src() inaccessiblememonly {
  call void @fn()
  ret i4 0
}

define i4 @tgt() inaccessiblememonly {
  unreachable
}

declare void @fn()
