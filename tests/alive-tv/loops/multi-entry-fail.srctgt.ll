; TEST-ARGS: -src-unroll=2 -tgt-unroll=2
; ERROR: Value mismatch

declare i1 @cond()

define ptr @src(ptr %a) {
entry:
  br label %while1

while1:
  %p.0 = phi ptr [ %a, %entry ], [ %p.0.next, %while1 ]
  %p.0.next = getelementptr i8, ptr %p.0, i32 1
  %cmp = call i1 @cond()
  br i1 %cmp, label %while2, label %while1

while2:
  %i = phi i32 [ %i.next, %while2.latch ], [ 0, %while1 ]
  %p.1 = phi ptr [ %p.1.next, %while2.latch ], [ %p.0, %while1 ]
  %i.next = add i32 %i, 1
  %p.1.next = getelementptr i8, ptr %p.1, i32 1
  %cmp3 = icmp eq i32 2, %i
  br i1 %cmp3, label %return, label %while2.latch

while2.latch:
  br label %while2

return:
  ret ptr %p.1
}

define ptr @tgt(ptr %a) {
entry:
  br label %while1

while1:
  %p.0 = phi ptr [ %a, %entry ], [ %p.0.next, %while1 ]
  %p.0.next = getelementptr i8, ptr %p.0, i32 1
  %cmp = call i1 @cond()
  br i1 %cmp, label %while2, label %while1

while2:
  %i = phi i32 [ %i.next, %while2.latch ], [ 0, %while1 ]
  %p.1 = getelementptr i8, ptr %p.0, i32 %i
  %i.next = add i32 %i, 1
  %cmp3 = icmp eq i32 2, %i
  br i1 %cmp3, label %return, label %while2.latch

while2.latch:
  br label %while2

return:
  ret ptr %p.0
}
