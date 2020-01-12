define i32 @f(i32 %n) {
entry:
  %cmp = icmp eq i32 %n, 0
  br i1 %cmp, label %end, label %loop

loop:
  %i = phi i32 [ %i2, %loop ], [ 0, %entry ]
  %r = phi i32 [ %r2, %loop ], [ 0, %entry ]
  %i2 = add i32 %i, 1
  %r2 = add i32 %r, 2
  %c2 = icmp eq i32 %i2, %n
  br i1 %c2, label %end, label %loop

end:
  %ret = phi i32 [ 0, %entry ], [ %r2, %loop ]
  ret i32 %ret
}

define i32 @f2(i32 %n) {
entry:
  %cmp = icmp eq i32 %n, 0
  br i1 %cmp, label %end, label %loop

loop:
  %i = phi i32 [ %i2, %loop ], [ 0, %entry ]
  %r = phi i32 [ %r2, %loop ], [ 0, %entry ]
  %i2 = add i32 %i, 1
  %r2 = add i32 %r, 2
  %c2 = icmp eq i32 %i2, %n
  br i1 %c2, label %end, label %loop

end:
  %ret = phi i32 [ 0, %entry ], [ %r2, %loop ]
  ret i32 %ret
}
