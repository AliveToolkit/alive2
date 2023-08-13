; TEST-ARGS: -src-unroll=2 -tgt-unroll=2

define i32 @src() {
entry:
  %r = alloca i32, align 4
  store i32 0, ptr %r, align 4
  br label %for.cond

for.cond:
  %0 = phi i32 [ 0, %entry ], [ %inc6, %for.inc5 ]
  %cmp = icmp slt i32 %0, 2
  br i1 %cmp, label %for.body, label %for.end7

for.body:
  br label %for.cond1

for.cond1:
  %1 = phi i32 [ 0, %for.body ], [ %inc4, %for.inc ]
  %cmp2 = icmp slt i32 %1, 2
  br i1 %cmp2, label %for.body3, label %for.inc5

for.body3:
  %2 = load i32, ptr %r, align 4
  %inc = add i32 %2, 1
  store i32 %inc, ptr %r, align 4
  br label %for.inc

for.inc:
  %inc4 = add i32 %1, 1
  br label %for.cond1

for.inc5:
  %inc6 = add i32 %0, 1
  br label %for.cond

for.end7:
  %3 = load i32, ptr %r, align 4
  ret i32 %3
}

define i32 @tgt() {
  ret i32 4
}
