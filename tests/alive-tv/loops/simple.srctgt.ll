; TEST-ARGS: -src-unroll=2 -tgt-unroll=2

define i32 @src() {
entry:
  %r = alloca i32, align 4
  store i32 0, ptr %r, align 4
  br label %for.cond

for.cond:
  %i = phi i32 [ 0, %entry ], [ %inc1, %for.body ]
  %cmp = icmp slt i32 %i, 2
  br i1 %cmp, label %for.body, label %for.end

for.body:
  %0 = load i32, ptr %r, align 4
  %inc = add i32 %0, 1
  store i32 %inc, ptr %r, align 4
  %inc1 = add i32 %i, 1
  br label %for.cond

for.end:
  %1 = load i32, ptr %r, align 4
  ret i32 %1
}

define i32 @tgt() {
  ret i32 2
}
