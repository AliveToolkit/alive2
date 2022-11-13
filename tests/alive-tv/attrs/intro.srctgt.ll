define i8 @src(ptr %p) {
  %v = load i8, ptr %p
  ret i8 %v
}

define i8 @tgt(ptr %p) memory(read) {
  %v = load i8, ptr %p
  ret i8 %v
}
