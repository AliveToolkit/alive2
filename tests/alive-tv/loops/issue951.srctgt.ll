; ERROR: Source is more defined than target
; TEST-ARGS: -src-unroll=3 -tgt-unroll=3

define i8 @src(i8 %v) {
entry:
  %v.nonneg = icmp sgt i8 %v, -1
  br label %for.body

for.body:
  %sum = phi i8 [ 0, %entry ], [ %sum.next, %for.body ]
  %sum.next = add i8 %sum, %v
  %overflow = icmp ult i8 %sum.next, %sum
  %cmp.i5.i = xor i1 %v.nonneg, %overflow
  call void @use(i1 %cmp.i5.i)
  br i1 %cmp.i5.i, label %for.body, label %cleanup1.loopexit

cleanup1.loopexit:
  %cmp.not.lcssa.ph = phi i8 [ %sum, %for.body ]
  ret i8 %cmp.not.lcssa.ph
}

define i8 @tgt(i8 %v) {
entry:
  %v.nonneg = icmp sgt i8 %v, -1
  br label %for.body

for.body:
  %sum = phi i8 [ 0, %entry ], [ %sum.next, %for.body ]
  %sum.next = add i8 %sum, %v
  call void @use(i1 %v.nonneg)
  br i1 %v.nonneg, label %for.body, label %cleanup1.loopexit

cleanup1.loopexit:
  %cmp.not.lcssa.ph = phi i8 [ %sum, %for.body ]
  ret i8 %cmp.not.lcssa.ph
}

declare void @use(i1)
