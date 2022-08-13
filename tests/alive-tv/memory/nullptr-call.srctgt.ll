define ptr @src(i1 %arg) {
bb:
  %tmp = alloca i64, align 8
  br i1 %arg, label %bb2, label %bb6

bb2:
  %tmp3 = call ptr @f(ptr %tmp)
  switch i1 false, label %bb8 [
    i1 true, label %bb6
  ]

bb6:
  %tmp7 = phi ptr [ null, %bb ], [ %tmp3, %bb2 ]
  ret ptr %tmp7

bb8:
  unreachable
}

define ptr @tgt(i1 %arg) {
bb:
  %tmp = alloca i64, align 8
  br i1 %arg, label %bb2, label %bb6

bb2:
  %tmp3 = call ptr @f(ptr %tmp)
  br label %bb6

bb6:
  %tmp7 = phi ptr [ null, %bb ], [ %tmp3, %bb2 ]
  ret ptr %tmp7
}

declare ptr @f(ptr)
