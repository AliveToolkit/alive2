; TEST-ARGS: -tgt-is-asm
; SKIP-IDENTITY

define void @src(ptr %foo2) {
  store <2 x i10> splat (i10 -5), ptr %foo2, align 4
  ret void
}

define void @tgt(ptr %0) {
  %n1 = getelementptr i8, ptr %0, i64 2
  store i8 15, ptr %n1, align 1
  store i16 -4101, ptr %0, align 1
  ret void
}
