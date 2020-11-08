; TEST-ARGS: -src-unroll=2 -tgt-unroll=2

declare i1 @cond()

define i8* @src(i8* %a) {
entry:
  br label %while1

while1:
  %p.0 = phi i8* [ %a, %entry ], [ %p.0.next, %while1 ]
  %p.0.next = getelementptr i8, i8* %p.0, i32 1
  %cmp = call i1 @cond()
  br i1 %cmp, label %while2, label %while1

while2:
  %i = phi i32 [ %i.next, %while2.body ], [ 0, %while1 ]
  %p.1 = phi i8* [ %p.1.next, %while2.body ], [ %p.0, %while1 ]
  %i.next = add i32 %i, 1
  %p.1.next = getelementptr i8, i8* %p.1, i32 1
  %cmp3 = icmp eq i32 2, %i
  br i1 %cmp3, label %return, label %while2.body

while2.body:
  br label %while2

return:
  ret i8* %p.1
}

define i8* @tgt(i8* %a) {
entry:
  br label %while1

while1:
  %p.0 = phi i8* [ %a, %entry ], [ %p.0.next, %while1 ]
  %p.0.next = getelementptr i8, i8* %p.0, i32 1
  %cmp = call i1 @cond()
  br i1 %cmp, label %while2, label %while1

while2:
  %i = phi i32 [ %i.next, %while2.body ], [ 0, %while1 ]
  %p.1 = getelementptr i8, i8* %p.0, i32 %i
  %i.next = add i32 %i, 1
  %cmp3 = icmp eq i32 2, %i
  br i1 %cmp3, label %return, label %while2.body

while2.body:
  br label %while2

return:
  ret i8* %p.1
}


