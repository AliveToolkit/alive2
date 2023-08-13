; TEST-ARGS: -src-unroll=3 -tgt-unroll=3
; ERROR: Value mismatch

define i32 @src() {
entry:
  %i = alloca i32, align 4
  store i32 0, ptr %i, align 4
  br label %header

header:
  %ph = phi i32 [ %inc, %header ], [ 0, %entry ]
  %inc = add i32 %ph, 1
  store i32 %inc, ptr %i, align 4
  %cmp2 = icmp sge i32 %inc, 3
  br i1 %cmp2, label %exit, label %header

exit:
  %l = load i32, ptr %i, align 4
  ret i32 %l
}

define i32 @tgt() {
  ret i32 2
}
