; TEST-ARGS: -tgt-is-asm

define i1 @src(ptr %0) {
  %2 = getelementptr i8, ptr %0, i64 -1
  %3 = icmp ult ptr %2, %0
  ret i1 %3
}

define i1 @tgt(ptr %0) {
  %a4_5.not.not = icmp ne ptr %0, null
  ret i1 %a4_5.not.not
}
