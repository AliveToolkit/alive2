; TEST-ARGS: -src-unroll=2

@a = global i32 0, align 4
define void @src() {
entry:
  br label %for.cond
for.cond:                                         ; preds = %for.cond, %entry
  %0 = load i32, i32* @a, align 4
  %tobool.not = icmp eq i32 %0, 0
  br i1 %tobool.not, label %for.cond1, label %for.cond
for.cond1:                                        ; preds = %for.cond, %for.inc
  %c.0 = phi i32 [ %add, %for.inc ], [ undef, %for.cond ]
  %tobool2.not = icmp eq i32 %c.0, 0
  br i1 %tobool2.not, label %for.cond.cleanup, label %for.inc
for.cond.cleanup:                                 ; preds = %for.cond1
  ret void
for.inc:                                          ; preds = %for.cond1
  %add = add nsw i32 %c.0, %0
  br label %for.cond1
}

define void @tgt() {
  ret void
}
