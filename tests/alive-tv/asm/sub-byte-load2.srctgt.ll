; TEST-ARGS: -tgt-is-asm

define i8 @src(ptr %0) {
  %2 = load i1, ptr %0, align 1
  %3 = load i8, ptr %0, align 1
  ret i8 %3
}

define i8 @tgt(ptr %0) {
  %r = load i8, ptr %0, align 1
  ret i8 %r
}
