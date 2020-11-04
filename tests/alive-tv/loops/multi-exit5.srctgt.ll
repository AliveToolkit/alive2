; TEST-ARGS: -src-unroll=3 -tgt-unroll=3

define i32 @src() {
entry:
  br label %for.cond

for.cond:
  %i = phi i32 [ 0, %entry ], [ %inc1, %for.body2 ]
  %r = phi i32 [ 0, %entry ], [ %inc3, %for.body2 ]
  %cmp = icmp uge i32 %i, 4
  br i1 %cmp, label %for.end2, label %for.body

for.body:
  %inc = add i32 %r, 2
  %inc1 = add i32 %i, 1
  %cmp2 = icmp eq i32 %inc, 6
  br i1 %cmp2, label %for.end1, label %for.body2

for.body2:
  %inc3 = add i32 %inc, 1
  %cmp3 = icmp eq i32 %inc3, 9
  br i1 %cmp3, label %for.end3, label %for.cond

for.end1:
  br label %exit

for.end2:
  br label %exit

for.end3:
  br label %exit

exit:
  %p = phi i32 [%r, %for.end1], [%r, %for.end2], [%inc3, %for.end3]
  ret i32 %p
}

define i32 @tgt() {
  ret i32 9
}
