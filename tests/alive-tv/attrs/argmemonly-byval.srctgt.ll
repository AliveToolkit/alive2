define void @src(ptr byval(i32) %0) {
  %3 = load i32, ptr %0
  ret void
}

define void @tgt(ptr byval(i32) captures(none) %0) memory(argmem:readwrite) {
  %3 = load i32, ptr %0
  ret void
}
