define void @src(ptr %p) {
  store {} {}, ptr %p
  ret void
}

define void @tgt(ptr %p) {
  ret void
}
