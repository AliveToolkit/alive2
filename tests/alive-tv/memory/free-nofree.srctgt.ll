declare void @free(ptr) memory(none)

define void @src(ptr %0) {
  call void @free(ptr %0)
  ret void
}

define void @tgt(ptr %0) {
  ret void
}
