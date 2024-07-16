; TEST-ARGS: -tgt-is-asm

define i1 @src(ptr %0) {
  %2 = load i9, ptr %0, align 4
  %3 = icmp slt i9 %2, 0
  ret i1 %3
}

define i1 @tgt(ptr %0)  {
  %p = getelementptr i8, ptr %0, i64 1
  %a3_1 = load i8, ptr %p, align 1
  %a6_23 = trunc i8 %a3_1 to i1
  ret i1 %a6_23
}
