; TEST-ARGS: -src-unroll=2 -tgt-unroll=2

define i32 @src() {
  br label %1

1:
  %2 = phi i32 [ 0, %0 ], [ 1, %6 ]
  br label %3

3:
  %4 = mul i32 %2, 1
  br label %5

5:
  br i1 false, label %3, label %6

6:
  %7 = icmp slt i32 %2, 1
  br i1 %7, label %1, label %8

8:
  ret i32 %4
}

define i32 @tgt() {
  ret i32 1
}
