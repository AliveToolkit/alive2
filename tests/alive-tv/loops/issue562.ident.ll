; TEST-ARGS: -tgt-unroll=1
define i64 @f(i32 %x) {
entry:
  br label %loop

BB1:
  br label %BB2

BB2:
  %ret = phi i64 [ %j, %BBif ], [ -1, %BB1 ]
  ret i64 %ret

loop:
  %j = phi i64 [ 0, %BBif ], [ 2, %entry ]
  %cmp = icmp sgt i32 %x, -1
  br i1 %cmp, label %BB1, label %BBif

BBif:
  br i1 true, label %BB2, label %loop
}
