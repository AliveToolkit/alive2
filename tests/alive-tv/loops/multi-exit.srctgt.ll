; TEST-ARGS: -src-unroll=2 -tgt-unroll=2

define i32 @src() {
entry:
  br label %for.cond

for.cond:
  %i = phi i32 [ 0, %entry ], [ %inc1, %for.body ]
  %r = phi i32 [ 0, %entry ], [ %inc, %for.body ]
  switch i32 %i, label %for.body [
    i32 2, label %for.end
    i32 3, label %for.end
  ]

for.body:
  %inc = add i32 %r, 2
  %inc1 = add i32 %i, 1
  br label %for.cond

for.end:
  ret i32 %r
}

define i32 @tgt() {
  ret i32 4
}
