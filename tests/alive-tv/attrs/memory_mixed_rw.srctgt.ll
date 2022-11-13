define void @src(ptr %p) {
  call void @fn(ptr %p)
  call void @fn(ptr %p)
  ret void
}

define void @tgt(ptr %p) {
  call void @fn(ptr %p)
  ret void
}

declare void @fn(ptr) memory(inaccessiblemem: write, argmem:read)
