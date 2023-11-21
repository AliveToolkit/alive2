define i32 @src(ptr %0) memory(read, argmem: none) {
  %2 = load i32, ptr %0, align 4
  ret i32 %2
}

define i32 @tgt(ptr %0) memory(read, argmem: none) {
  unreachable
}
