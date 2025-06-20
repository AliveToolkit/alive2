; TEST-ARGS: -tgt-is-asm
@a = external global ptr

define void @src() {
  %1 = load ptr, ptr @a, align 8
  store ptr %1, ptr @a, align 8
  ret void
}

define void @tgt() {
  ret void
}
