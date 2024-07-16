; TEST-ARGS: -tgt-is-asm

define i8 @src(ptr %p) {
  %x = load i5, ptr %p, align 2
  %y = zext i5 %x to i8
  ret i8 %y
}

define i8 @tgt(ptr %p) {
  %x = load i8, ptr %p, align 2
  ret i8 %x
}
