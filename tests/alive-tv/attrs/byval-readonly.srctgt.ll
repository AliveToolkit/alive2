define void @src(ptr byval(i8) %x) memory(read) {
  store i8 3, ptr %x
  ret void
}

define void @tgt(ptr byval(i8) %x) memory(read) {
  unreachable
}
