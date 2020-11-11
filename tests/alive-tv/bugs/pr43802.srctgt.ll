define i32 @src(i32 %arg) {
bb:
  %tmp = sub nsw i32 0, %arg
  %tmp1 = icmp eq i32 %arg, -2147483648
  br i1 %tmp1, label %bb2, label %bb3

bb2:                                              ; preds = %bb
  br label %bb3

bb3:                                              ; preds = %bb2, %bb
  %tmp4 = phi i32 [ -2147483648, %bb2 ], [ %tmp, %bb ]
  ret i32 %tmp4
}

define i32 @tgt(i32 %arg) {
bb:
  %tmp = sub nsw i32 0, %arg
  %tmp1 = icmp eq i32 %arg, -2147483648
  br i1 %tmp1, label %bb2, label %bb3

bb2:                                              ; preds = %bb
  br label %bb3

bb3:                                              ; preds = %bb2, %bb
  ret i32 %tmp
}

; ERROR: Target is more poisonous than source
