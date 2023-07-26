; TEST-ARGS: -src-unroll=2 -tgt-unroll=2

define ptr @src(ptr %p, i32 %choice) {
entry:
  br label %while.cond

while.cond:
  %foo = phi ptr [ %p, %entry ], [ null, %while.body ]
  switch i32 %choice, label %while.body [
    i32 -1, label %while.end
    i32 40, label %land.end
  ]

land.end:
  br label %while.end

while.body:
  br label %while.cond

while.end:
  ret ptr %foo
}

define ptr @tgt(ptr %p, i32 %choice) {
  ret ptr %p
}
