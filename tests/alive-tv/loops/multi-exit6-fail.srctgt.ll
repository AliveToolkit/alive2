; TEST-ARGS: -src-unroll=4 -tgt-unroll=4
; ERROR: Value mismatch

define i8 @src(i8 %a, i1 %b, i1 %c) {
entry:
  br label %loop

loop:
  %x = phi i8 [ %a, %entry ], [ 0, %body ]
  br i1 %c, label %exit, label %body

body:
  br i1 %b, label %loop, label %exit

exit:
  %y = phi i8 [ %x, %body ],  [ %x, %loop ]
  ret i8 %y
}

define i8 @tgt(i8 %a, i1 %b, i1 %c) {
entry:
  br label %loop

loop:
  %x = phi i8 [ %a, %entry ], [ 0, %body ]
  br i1 %c, label %exit, label %body

body:
  br i1 %b, label %loop, label %exit

exit:
  %y = add i8 %x, 1
  ret i8 %y
}
