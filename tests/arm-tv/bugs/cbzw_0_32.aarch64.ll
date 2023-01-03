; TEST-ARGS: --disable-undef-input --disable-poison-input


define i32 @cttz_zero_check(i32 %0) {
  %2 = icmp eq i32 %0, 0
  br i1 %2, label %11, label %3

3:                                                ; preds = %1
  br label %4

4:                                                ; preds = %4, %3
  %5 = phi i32 [ %8, %4 ], [ 0, %3 ]
  %6 = phi i32 [ %7, %4 ], [ %0, %3 ]
  %7 = shl i32 %6, 1
  %8 = add nsw i32 %5, 1
  %9 = icmp eq i32 %7, 0
  br i1 %9, label %10, label %4

10:                                               ; preds = %4
  br label %11

11:                                               ; preds = %10, %1
  %12 = phi i32 [ 0, %1 ], [ %8, %10 ]
  ret i32 %12
}
