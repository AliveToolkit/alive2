; TEST-ARGS: -src-unroll=1 -tgt-unroll=1

define i32 @src(i32 %t) {
entry:
  br label %loop

loop:
  %i1.i64.0 = phi i64 [ 0, %entry ], [ %nextivloop, %ifmerge.46 ]
  %cmp.38 = icmp sgt i32 2, %t
  br i1  %cmp.38, label %loop.exit, label %ifmerge.38

ifmerge.38:
  %cmp = icmp sgt i32 0, %t
  br i1 %cmp, label %ifmerge, label %ifmerge.46

ifmerge.46:
  %nextivloop = add i64 %i1.i64.0, 1
  %condloop = icmp ult i64 %nextivloop, 2
  br i1 %condloop, label %loop, label %for.end

ifmerge:
  br label %loop.exit

loop.exit:
  %ph = phi i64 [ %i1.i64.0, %ifmerge ], [ 0, %loop ]
  br label %for.end

for.end:
  ret i32 0
}

define i32 @tgt(i32 %t) {
  ret i32 0
}
