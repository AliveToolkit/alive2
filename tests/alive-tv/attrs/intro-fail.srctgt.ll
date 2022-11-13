; ERROR: Source is more defined than target

define void @src(ptr %p) {
  store i8 1, ptr %p
  ret void
}

define void @tgt(ptr %p) memory(read) {
  store i8 1, ptr %p
  ret void
}
