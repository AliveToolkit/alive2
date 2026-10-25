; TEST-ARGS: -src-unroll=8 -tgt-unroll=8
; ERROR: Value mismatch

; A loop whose body is a single basic block used not to be registered as a loop
; header in the loop forest, so the unroller skipped it when it was nested in
; another loop. Its backedge then survived into the unrolled function, where it
; is redirected to #sink, so the executions that run the inner loop more than
; once dropped out of the refinement query and this pair verified.
; tgt returns 3 * %n, not %n.

define i32 @src(i32 %n) {
entry:
  %c = icmp slt i32 %n, 1
  br i1 %c, label %zero, label %pos

zero:
  ret i32 0

pos:
  ret i32 %n
}

define i32 @tgt(i32 %n) {
entry:
  %c = icmp slt i32 %n, 1
  br i1 %c, label %done, label %outer

outer:
  %i = phi i32 [ 0, %entry ], [ %i.next, %latch ]
  %acc = phi i32 [ 0, %entry ], [ %acc.next, %latch ]
  br label %inner

inner:
  %j = phi i32 [ 0, %outer ], [ %j.next, %inner ]
  %a = phi i32 [ %acc, %outer ], [ %a.next, %inner ]
  %a.next = add i32 %a, 1
  %j.next = add i32 %j, 1
  %jc = icmp slt i32 %j.next, 3
  br i1 %jc, label %inner, label %latch

latch:
  %acc.next = phi i32 [ %a.next, %inner ]
  %i.next = add i32 %i, 1
  %ic = icmp slt i32 %i.next, %n
  br i1 %ic, label %outer, label %done

done:
  %r = phi i32 [ 0, %entry ], [ %acc.next, %latch ]
  ret i32 %r
}
