; TEST-ARGS: -src-unroll=2 -tgt-unroll=2
; ERROR: Value mismatch

define i32 @src() {
entry:
  br label %for.cond

for.cond:
  %i = phi i32 [ 0, %entry ], [ %inc1, %for.body ]
  %r = phi i32 [ 0, %entry ], [ %inc, %for.body ]
  %cmp = icmp slt i32 %i, 2
  br i1 %cmp, label %for.body, label %for.end

for.body:
  %inc = add i32 %r, 2
  %inc1 = add i32 %i, 1
  br label %for.cond

for.end:
  %0 = phi i32 [ %r, %for.cond ]
  ret i32 %0
}

define i32 @tgt() {
  ret i32 2
}
