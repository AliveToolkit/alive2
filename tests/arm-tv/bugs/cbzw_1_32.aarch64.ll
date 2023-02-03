; TEST-ARGS:

define i32 @cttz_zero_check(i32 %0, i1 %1) {
  %3 = icmp eq i32 %0, 0
  br i1 %3, label %8, label %4

4:                                                ; preds = %2
  br label %5

5:                                                ; preds = %5, %4
  %6 = icmp eq i32 0, 0
  br i1 %1, label %7, label %5

7:                                                ; preds = %5
  br label %8

8:                                                ; preds = %7, %2
  %9 = phi i32 [ 0, %2 ], [ 1, %7 ]
  ret i32 %9
}