; TEST-ARGS: -src-unroll=2 -tgt-unroll=2

define i8* @src(i8* %p, i32 %choice) {
entry:
  br label %while.cond

while.cond:
  %foo = phi i8* [ %p, %entry ], [ null, %while.body ]
  switch i32 %choice, label %while.body [
    i32 -1, label %while.end
    i32 40, label %land.end
  ]

land.end:
  br label %while.end

while.body:
  br label %while.cond

while.end:
  ret i8* %foo
}

define i8* @tgt(i8* %p, i32 %choice) {
  ret i8* %p
}
