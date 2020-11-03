; TEST-ARGS: -src-unroll=3 -tgt-unroll=3

define i32 @src() {
entry:
  br label %for.cond

for.cond:
  %i = phi i32 [ 0, %entry ], [ %inc1, %for.body ]
  %r = phi i32 [ 0, %entry ], [ %inc, %for.body ]
  %cmp = icmp uge i32 %i, 4
  br i1 %cmp, label %for.end, label %for.body

for.body:
  %inc = add i32 %r, 2
  %inc1 = add i32 %i, 1
  %cmp2 = icmp eq i32 %inc, 6
  br i1 %cmp2, label %for.end, label %for.cond

for.end:
  ret i32 %r
}

define i32 @tgt() {
  ret i32 4
}
