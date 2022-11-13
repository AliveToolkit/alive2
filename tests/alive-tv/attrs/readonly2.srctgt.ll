define i8 @src(ptr %p) memory(read) {
  store i8 0, ptr %p
  ret i8 2
}

define i8 @tgt(ptr %p) memory(read) {
  unreachable
}
