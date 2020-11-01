; TEST-ARGS: -src-unroll=3 -tgt-unroll=3
; ERROR: Value mismatch

define i32 @src() {
entry:
  %i = alloca i32, align 4
  store i32 0, i32* %i, align 4
  br label %while.body

while.body:
  %0 = load i32, i32* %i, align 4
  %inc = add i32 %0, 1
  store i32 %inc, i32* %i, align 4
  switch i32 %0, label %sw.default [
    i32 2, label %foo
    i32 3, label %bar
  ]


sw.default:
  br label %while.body

foo:
  ret i32 2

bar:
  ret i32 3
}

define i32 @tgt() {
  ret i32 3
}
