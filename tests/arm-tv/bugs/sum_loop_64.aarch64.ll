; TEST-ARGS:

target datalayout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128"
target triple = "aarch64"

define i64 @sum(i64 %0) #0 {
  br label %2

2:                                                ; preds = %4, %1
  %.01 = phi i64 [ 0, %1 ], [ %5, %4 ]
  %.0 = phi i64 [ 1, %1 ], [ %6, %4 ]
  %3 = icmp slt i64 %.0, %0
  br i1 %3, label %4, label %7

4:                                                ; preds = %2
  %5 = add nsw i64 %.01, %.0
  %6 = add nsw i64 %.0, 1
  br label %2

7:                                                ; preds = %2
  ret i64 %.01
}
