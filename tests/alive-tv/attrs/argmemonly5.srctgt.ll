@x = external global i8

define i8 @src(ptr %0) memory(read, argmem: none) {
  %2 = load i8, ptr @x, align 1
  ret i8 %2
}

define i8 @tgt(ptr %0) memory(none) {
  unreachable
}

; ERROR: Source is more defined than target
