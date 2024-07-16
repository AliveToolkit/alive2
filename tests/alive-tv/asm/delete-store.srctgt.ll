; TEST-ARGS: -tgt-is-asm
; SKIP-IDENTITY

define void @src(ptr %0) {
  %2 = load i1, ptr %0, align 1
  store i1 %2, ptr %0, align 1
  ret void
}

define void @tgt(ptr %0) {
  ret void
}
