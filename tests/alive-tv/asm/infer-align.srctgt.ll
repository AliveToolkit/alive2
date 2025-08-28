; TEST-ARGS: -tgt-is-asm

define void @src(ptr %0, ptr %1) {
  store i16 0, ptr %0, align 16
  store i16 0, ptr %1, align 8
  store i16 0, ptr %0, align 2
  ret void
}

define void @tgt(ptr %0, ptr %1) {
  store i16 0, ptr %1, align 1
  store i16 0, ptr %0, align 1
  ret void
}
