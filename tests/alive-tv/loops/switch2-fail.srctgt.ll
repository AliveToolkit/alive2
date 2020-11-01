; TEST-ARGS: -src-unroll=4 -tgt-unroll=4
; ERROR: Value mismatch

define i32 @src(i1 %c) {
entry:
  %i = alloca i32, align 4
  store i32 0, i32* %i, align 4
  br label %while.body

while.body:
  %0 = load i32, i32* %i, align 4
  %inc = add i32 %0, 1
  store i32 %inc, i32* %i, align 4
  switch i32 %0, label %sw.default [
    i32 2, label %sw.2
    i32 3, label %sw.3
  ]

sw.2:
  br i1 %c, label %while.body, label %foo

sw.3:
  br i1 %c, label %bar, label %while.body

sw.default:
  br label %while.body

foo:
  ret i32 2

bar:
  ret i32 3
}

define i32 @tgt(i1 %c) {
  %ret = select i1 %c, i32 5, i32 2
  ret i32 %ret
}
