; TEST-ARGS: -src-unroll=2 -tgt-unroll=2

define i32 @src() {
entry:
  br label %head1

head1:
  %i0 = phi i32 [ 0, %entry ], [ %i.next, %body_2 ]
  br label %body_1

body_1:
  %i = phi i32 [ %i.next, %body_2 ], [ %i0, %head1 ]
  %i.next = add i32 %i, 1
  %cond = icmp sgt i32 %i.next, 2
  br i1 %cond, label %exit, label %body_2

body_2:
  %cond2 = icmp sgt i32 %i.next, 1
  br i1 %cond2, label %body_1, label %head1

exit:
  ret i32 %i
}

define i32 @tgt() {
  ret i32 2
}
