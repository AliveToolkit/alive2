; TEST-ARGS: -tgt-is-asm

define i8 @src() {
  ret i8 undef
}

define i8 @tgt() {
  %p = alloca i8
  %v = load i8, ptr %p
  ret i8 %v
}
