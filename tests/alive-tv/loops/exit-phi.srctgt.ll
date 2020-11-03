; TEST-ARGS: -src-unroll=2 -tgt-unroll=2

define i8 @src(i1 %c, i8 %a, i8 %b) {
entry:
  br i1 %c, label %exit, label %for.cond

for.cond:
  %i = phi i32 [ 0, %entry ], [ %inc, %for.body ]
  %cmp = icmp slt i32 %i, 2
  br i1 %cmp, label %for.body, label %exit

for.body:
  %inc = add i32 %i, 1
  br label %for.cond

exit:
  %r = phi i8 [ %a, %entry ], [ %b, %for.cond ]
  ret i8 %r
}

define i8 @tgt(i1 %c, i8 %a, i8 %b) {
  %r = select i1 %c, i8 %a, i8 %b
  ret i8 %r
}
