; TEST-ARGS: -tgt-is-asm
; SKIP-IDENTITY

declare ptr @llvm.ptrmask.p0.i64(ptr, i64)

define i8 @src(ptr %0) {
  %2 = call ptr @llvm.ptrmask.p0.i64(ptr %0, i64 -9223372036854775808)
  %3 = load i8, ptr %2, align 1
  ret i8 %3
}

define i8 @tgt(ptr %0) {
entry:
  %1 = ptrtoint ptr %0 to i64
  %a3_2 = and i64 %1, -9223372036854775808
  %2 = inttoptr i64 %a3_2 to ptr
  %a4_22 = load i8, ptr %2, align 1
  ret i8 %a4_22
}
