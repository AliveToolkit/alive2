; TEST-ARGS: -tgt-is-asm

declare i32 @memcmp(ptr, ptr, i64)

define i1 @src(ptr align 4 %0, ptr %intbuf) {
  %2 = call i32 @memcmp(ptr %0, ptr %intbuf, i64 1)
  %3 = icmp eq i32 %2, 0
  ret i1 %3
}

define i1 @tgt(ptr align 4 %0, ptr %1) {
  %a3_1 = load i8, ptr %0, align 1
  %a4_1 = load i8, ptr %1, align 1
  %a5_8.not = icmp eq i8 %a3_1, %a4_1
  ret i1 %a5_8.not
}
