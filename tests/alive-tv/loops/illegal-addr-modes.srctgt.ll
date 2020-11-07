; TEST-ARGS: -src-unroll=2 -tgt-unroll=2
target datalayout = "e-m:e-p:32:32-i64:64-v128:64:128-a:0:32-n32-S64"

define void @src(i8* %a) {
entry:
  br label %while.cond

while.cond:
  %p.0 = phi i8* [ %a, %entry ], [ %incdec.ptr, %while.cond ]
  %incdec.ptr = getelementptr i8, i8* %p.0, i32 1
  %0 = load i8, i8* %p.0, align 1
  %cmp = icmp eq i8 %0, 0
  br i1 %cmp, label %while.cond2, label %while.cond

while.cond2:
  %i = phi i32 [ %i.next, %while.body5 ], [ 0, %while.cond ]
  %p.1 = phi i8* [ %p.1.next, %while.body5 ], [ %p.0, %while.cond ]
  %cmp3 = icmp eq i32 2, %i
  br i1 %cmp3, label %while.end8, label %while.body5

while.body5:
  %i.next = add i32 %i, 1
  store i8 1, i8* %p.1, align 1
  %p.1.next = getelementptr i8, i8* %p.1, i32 1
  br label %while.cond2

while.end8:
  ret void
}

define void @tgt(i8* %a) {
entry:
  br label %while.cond

while.cond:
  %p.0 = phi i8* [ %a, %entry ], [ %incdec.ptr, %while.cond ]
  %incdec.ptr = getelementptr i8, i8* %p.0, i32 1
  %0 = load i8, i8* %p.0, align 1
  %cmp = icmp eq i8 %0, 0
  br i1 %cmp, label %while.cond2, label %while.cond

while.cond2:
  %i = phi i32 [ %i.next, %while.body5 ], [ 0, %while.cond ]
  %p.1 = getelementptr i8, i8* %p.0, i32 %i
  %cmp3 = icmp eq i32 2, %i
  br i1 %cmp3, label %while.end8, label %while.body5

while.body5:
  %i.next = add i32 %i, 1
  store i8 1, i8* %p.1, align 1
  br label %while.cond2

while.end8:
  ret void
}


