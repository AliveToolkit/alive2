; TEST-ARGS: -src-unroll=4 -tgt-unroll=4
; ERROR: Value mismatch

define i32 @src() {
entry:
  br label %for.cond

for.cond:
  %i = phi i32 [ 0, %entry ], [ %inc1, %for.body ]
  %r = phi i32 [ 0, %entry ], [ %inc, %for.body ]
  %cmp = icmp uge i32 %i, 4
  br i1 %cmp, label %for.end2, label %for.body

for.body:
  %inc = add i32 %r, 2
  %inc1 = add i32 %i, 1
  %cmp2 = icmp eq i32 %inc, 8
  br i1 %cmp2, label %for.end1, label %for.cond

for.end1:
  br label %pre.exit

for.end2:
  br label %pre.exit

pre.exit:
  br label %exit

exit:
  ret i32 %r
}

define i32 @tgt() {
  ret i32 0
}
