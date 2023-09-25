; TEST-ARGS: -tgt-is-asm
; SKIP-IDENTITY

define void @src(ptr %0) {
  %2 = load ptr, ptr %0, align 8
  store i64 0, ptr %2, align 8
  ret void
}

define void @tgt(ptr %0) {
  %x = load i64, ptr %0, align 8
  %y = inttoptr i64 %x to ptr
  store i64 0, ptr %y, align 8
  ret void
}
