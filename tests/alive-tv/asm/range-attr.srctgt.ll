; TEST-ARGS: -tgt-is-asm

define i1 @src(i32 range(i32 0, 12) %a, i1 %b) {
  %x = icmp eq i32 %a, 0
  %c = select i1 %b, i1 %x, i1 false
  ret i1 %c
}

define i1 @tgt(i32 range(i32 0, 12) %0, i1 %1) {
  %a3_7.not = icmp eq i32 %0, 0
  %a5_56 = and i1 %1, %a3_7.not
  ret i1 %a5_56
}
