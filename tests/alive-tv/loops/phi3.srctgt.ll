; TEST-ARGS: -src-unroll=3 -tgt-unroll=3

define i32 @src() {
entry:
  %i = alloca i32, align 4
  store i32 0, ptr %i, align 4
  br label %header

header:
  %ph = phi i32 [ %inc, %if.cont ], [ 0, %entry ]
  %inc = add i32 %ph, 1
  store i32 %inc, ptr %i, align 4
  %cmp2 = icmp sge i32 %inc, 3
  br i1 %cmp2, label %a, label %b

a:
  br label %if.cont

b:
  br label %if.cont

if.cont:
  %ph2 = phi i1 [ 1, %a ], [ 0, %b ]
  br i1 %ph2, label %exit, label %header

exit:
  %l = load i32, ptr %i, align 4
  ret i32 %l
}

define i32 @tgt() {
  ret i32 3
}
