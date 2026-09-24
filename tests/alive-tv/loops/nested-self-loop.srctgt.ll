; TEST-ARGS: -src-unroll=8 -tgt-unroll=8

; The counterpart of nested-self-loop-fail.srctgt.ll: unrolling a nested self
; loop must not make an equivalent pair unverifiable.

define i32 @src(i32 %n) {
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
  %a.next = sub i32 %a, -1
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
