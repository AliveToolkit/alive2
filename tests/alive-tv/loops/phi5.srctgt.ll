; TEST-ARGS: -src-unroll=5 -tgt-unroll=5

define i32 @src() {
entry:
  br label %loop
loop:
  %i = phi i32 [0, %entry], [%i.next, %loop]
  %j = phi i32 [0, %entry], [%i, %loop] ; should point to the previous iteration's %i
  %i.next = add i32 %i, 1
  %c = icmp ne i32 %i, 4
  br i1 %c, label %loop, label %exit
exit:
  ret i32 %j
}

define i32 @tgt() {
  ret i32 3
}
