; TEST-ARGS: -tgt-is-asm

define i8 @src(ptr %p) {
  %q = load ptr, ptr %p
  load i8, ptr %q, align 512
  ret i8 0
}

define i8 @tgt(ptr %p) {
  %p2 = getelementptr i8, ptr %p, i32 0
  %v = load i8, ptr %p2
  ret i8 %v
}
