; TEST-ARGS: -tgt-is-asm
; SKIP-IDENTITY
; ERROR: Mismatch in memory

define void @src(ptr %0) {
  store i9 5, ptr %0
  ret void
}

define void @tgt(ptr %0)  {
  store i16 1029, ptr %0
  ret void
}
