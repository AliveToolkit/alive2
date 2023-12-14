; SKIP-IDENTITY

declare void @c1()

define i8 @src(i8 %spec.select) {
  br label %4

1:
  call void @c1()
  br label %3

.unreachabledefault:
  unreachable

2:
  ret i8 0

3:
  %.2 = phi i8 [ 0, %1 ], [ %.48, %4 ]
  switch i8 %.2, label %4 [
    i8 0, label %1
    i8 1, label %2
  ]

4:
  %.48 = phi i8 [ 0, %3 ], [ %spec.select, %0 ]
  switch i8 %.48, label %.unreachabledefault [
    i8 0, label %3
    i8 1, label %1
  ]
}

define i8 @tgt(i8 %spec.select) {
  br label %1

1:
  call void @c1()
  br label %1
}
