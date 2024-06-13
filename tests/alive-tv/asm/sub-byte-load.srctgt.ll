; TEST-ARGS: -tgt-is-asm

define i16 @src(ptr %p) {
  %x = load i5, ptr %p, align 2
  %y = zext i5 %x to i16
  ret i16 %y
}

define i16 @tgt(ptr %p) {
  %x = load i16, ptr %p, align 2
  ret i16 %x
}
