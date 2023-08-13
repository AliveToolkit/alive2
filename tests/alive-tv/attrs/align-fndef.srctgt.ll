define void @src(ptr align(4) %p) {
  load i8, ptr %p
  ret void
}

define void @tgt(ptr align(4) %p) {
  load i8, ptr %p, align 4
  ret void
}
