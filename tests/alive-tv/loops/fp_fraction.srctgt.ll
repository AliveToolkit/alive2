; TEST-ARGS: -src-unroll=23
; XFAIL: Value mismatch
; requires creating phi in exit block

; https://godbolt.org/z/dKxzKx

; Function Attrs: norecurse nounwind readnone uwtable
define dso_local i32 @src(i32 %0) local_unnamed_addr #0 {
  br label %2

2:                                                ; preds = %2, %1
  %3 = phi i32 [ %0, %1 ], [ %6, %2 ]
  %4 = and i32 %3, 8388608
  %5 = icmp eq i32 %4, 0
  %6 = shl i32 %3, 1
  br i1 %5, label %2, label %7

7:                                                ; preds = %2
  ret i32 %3
}

; Function Attrs: nounwind readnone uwtable
define dso_local i32 @tgt(i32 %0) local_unnamed_addr #1 {
  %2 = and i32 %0, 8388608
  %3 = icmp eq i32 %2, 0
  br i1 %3, label %4, label %9

4:                                                ; preds = %1
  %5 = and i32 %0, 8388607
  %6 = tail call i32 @llvm.ctlz.i32(i32 %5, i1 true)
  %7 = add nsw i32 %6, -8
  %8 = shl i32 %0, %7
  br label %9

9:                                                ; preds = %4, %1
  %10 = phi i32 [ %0, %1 ], [ %8, %4 ]
  ret i32 %10
}

; Function Attrs: nofree nosync nounwind readnone speculatable willreturn
declare i32 @llvm.ctlz.i32(i32, i1 immarg) #2
