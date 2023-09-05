define void @src(ptr readonly byval(i8) %p) {
  store i8 0, ptr %p
  ret void
}

define void @tgt(ptr readonly byval(i8) %p) {
  unreachable
}
