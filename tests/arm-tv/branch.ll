target datalayout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128"
target triple = "aarch64"

define dso_local i32 @foo(i32 %0, i32 %1) local_unnamed_addr #0 {
  %3 = icmp sgt i32 %0, 0
  br i1 %3, label %4, label %7

4:                                                ; preds = %2
  %5 = add nuw nsw i32 %0, 1
  %6 = add nsw i32 %5, %1
  br label %15

7:                                                ; preds = %2
  %8 = icmp slt i32 %1, 0
  br i1 %8, label %9, label %12

9:                                                ; preds = %7
  %10 = xor i32 %0, -1
  %11 = add i32 %10, %1
  br label %15

12:                                               ; preds = %7
  %13 = mul i32 %0, 3
  %14 = mul i32 %13, %1
  br label %15

15:                                               ; preds = %12, %9, %4
  %16 = phi i32 [ %6, %4 ], [ %11, %9 ], [ %14, %12 ]
  ret i32 %16
}

attributes #0 = { mustprogress nofree norecurse nosync nounwind readnone uwtable willreturn "frame-pointer"="non-leaf" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="generic" "target-features"="+neon" }