; TEST-ARGS: -src-unroll=7 -tgt-unroll=7
; ERROR: Source is more defined than target

; https://bugs.llvm.org/show_bug.cgi?id=35406

define i32 @src(ptr %p, ptr %p1) {
entry:
  br label %loop1

loop1:
  %local_0_ = phi i32 [ 8, %entry ], [ %9, %loop2.exit ]
  %local_2_ = phi i32 [ 63864, %entry ], [ %local_2_43, %loop2.exit ]
  %local_3_ = phi i32 [ 51, %entry ], [ %local_3_44, %loop2.exit ]
  %0 = udiv i32 14, %local_0_
  %1 = icmp ugt i32 %local_0_, 14
  br i1 %1, label %exit, label %general_case24

general_case24:
  %2 = udiv i32 60392, %0
  br i1 false, label %loop2, label %loop2.exit

loop2:
  %local_1_56 = phi i32 [ %2, %general_case24 ], [ %3, %loop2 ]
  %local_2_57 = phi i32 [ 1, %general_case24 ], [ %7, %loop2 ]
  %3 = add i32 %local_1_56, -1
  %4 = load i64, ptr %p1, align 8
  %5 = sext i32 %3 to i64
  %6 = sub i64 %4, %5
  store i64 %6, ptr %p1, align 8
  %7 = add nuw nsw i32 %local_2_57, 1
  %8 = icmp ugt i32 %local_2_57, 7
  br i1 %8, label %loop2.exit, label %loop2

loop2.exit:
  %local_2_43 = phi i32 [ %local_2_, %general_case24 ], [ 9, %loop2 ]
  %local_3_44 = phi i32 [ %local_3_, %general_case24 ], [ %local_1_56, %loop2 ]
  %9 = add nuw nsw i32 %local_0_, 1
  %10 = icmp ugt i32 %local_0_, 129
  br i1 %10, label %exit, label %loop1

exit:
  ret i32 0
}

define i32 @tgt(ptr %p, ptr %p1) {
entry:
  br label %loop1

loop1:                                            ; preds = %loop2.exit, %entry
  %local_0_ = phi i32 [ 8, %entry ], [ %7, %loop2.exit ]
  %0 = udiv i32 14, %local_0_
  %1 = udiv i32 60392, %0
  %2 = zext i32 %1 to i64
  %3 = icmp ugt i32 %local_0_, 14
  br i1 %3, label %exit, label %general_case24

general_case24:                                   ; preds = %loop1
  br i1 false, label %loop2.preheader, label %loop2.exit

loop2.preheader:                                  ; preds = %general_case24
  br label %loop2

loop2:                                            ; preds = %loop2.preheader, %loop2
  %indvars.iv = phi i64 [ %2, %loop2.preheader ], [ %indvars.iv.next, %loop2 ]
  %local_2_57 = phi i32 [ %6, %loop2 ], [ 1, %loop2.preheader ]
  %indvars.iv.next = add nsw i64 %indvars.iv, -1
  %4 = load i64, ptr %p1, align 8
  %5 = sub i64 %4, %indvars.iv.next
  store i64 %5, ptr %p1, align 8
  %6 = add nuw nsw i32 %local_2_57, 1
  %exitcond = icmp eq i32 %6, 9
  br i1 %exitcond, label %loop2.exit.loopexit, label %loop2

loop2.exit.loopexit:                              ; preds = %loop2
  br label %loop2.exit

loop2.exit:                                       ; preds = %loop2.exit.loopexit, %general_case24
  %7 = add nuw nsw i32 %local_0_, 1
  br i1 false, label %exit, label %loop1

exit:                                             ; preds = %loop2.exit, %loop1
  ret i32 0
}
