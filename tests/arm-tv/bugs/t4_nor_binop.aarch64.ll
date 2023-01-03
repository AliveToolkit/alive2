; TEST-ARGS: --disable-undef-input --disable-poison-input

define i1 @t4_nor_binop(i8 %0, i8 %1, i8 %2, i1 %3) {
  %5 = icmp eq i8 %0, 0
  br i1 %5, label %6, label %11

6:                                                ; preds = %4
  %7 = sdiv i8 %1, %0
  %8 = icmp ne i8 %7, 0
  %9 = icmp ne i8 %1, 0
  %10 = and i1 %9, %8
  br label %12

11:                                               ; preds = %4
  br label %12

12:                                               ; preds = %11, %6
  %13 = phi i1 [ %10, %6 ], [ %3, %11 ]
  ret i1 %13
}