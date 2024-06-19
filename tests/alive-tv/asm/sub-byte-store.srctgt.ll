; TEST-ARGS: -tgt-is-asm
; SKIP-IDENTITY

define void @src(ptr %0) {
  store i9 5, ptr %0
  ret void
}

define void @tgt(ptr %0)  {
  store i16 5, ptr %0
  ret void
}
