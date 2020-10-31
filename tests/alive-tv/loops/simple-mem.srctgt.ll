; FIXME: we should find the bug with unroll=2
; TEST-ARGS: -src-unroll=3
; ERROR: Value mismatch

define i32 @src() {
entry:
  %r = alloca i32, align 4
  %i = alloca i32, align 4
  store i32 0, i32* %r, align 4
  store i32 0, i32* %i, align 4
  br label %for.cond

for.cond:
  %0 = load i32, i32* %i, align 4
  %cmp = icmp slt i32 %0, 2
  br i1 %cmp, label %for.body, label %for.end

for.body:
  %1 = load i32, i32* %r, align 4
  %inc = add i32 %1, 1
  store i32 %inc, i32* %r, align 4
  %inc1 = add i32 %0, 1
  store i32 %inc1, i32* %i, align 4
  br label %for.cond

for.end:
  %2 = load i32, i32* %r, align 4
  ret i32 %2
}

define i32 @tgt() {
  ret i32 0
}
