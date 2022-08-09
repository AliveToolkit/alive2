; TEST-ARGS: -disable-poison-input

define void @src(ptr %p) {
  ret void
}

define void @tgt(ptr %p) {
  ret void
}
