; TEST-ARGS: -src-unroll=3 -tgt-unroll=3
; ERROR: Value mismatch

define i32 @src() {
entry:
  %i = alloca i32, align 4
  %r = alloca i32, align 4
  store i32 0, i32* %i, align 4
  store i32 1, i32* %r, align 4
  br label %header

header:
  %0 = load i32, i32* %i, align 4
  %inc = add i32 %0, 1
  store i32 %inc, i32* %i, align 4
  %cmp = icmp sge i32 %inc, 5
  br i1 %cmp, label %unreach, label %if.cont

if.cont:
  %cmp2 = icmp sge i32 %inc, 3
  br i1 %cmp2, label %exit, label %if.end

if.end:
  %1 = load i32, i32* %r, align 4
  %inc1 = add i32 %1, 1
  store i32 %inc1, i32* %r, align 4
  br label %header

unreach:
  br label %header

exit:
  %2 = load i32, i32* %r, align 4
  ret i32 %2
}

define i32 @tgt() {
  ret i32 2
}
