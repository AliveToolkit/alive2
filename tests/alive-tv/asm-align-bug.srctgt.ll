; RUN: %alive-tv --tgt-is-asm %s

define i24 @src(ptr %p) {
  %v = load i24, ptr %p, align 4
  ret i24 %v
}

define i24 @tgt(ptr %p) {
  %v = load i24, ptr %p, align 1
  ret i24 %v
}
