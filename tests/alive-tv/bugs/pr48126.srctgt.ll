; Found by Alive2
; TEST-ARGS: -src-unroll=4 -tgt-unroll=4
; ERROR: Source is more defined than target

; https://bugs.llvm.org/show_bug.cgi?id=48126

@b = external global [2048 x i32], align 16

define i32 @src(i32* %b, i32 %n) #0 {
  %1 = icmp sgt i32 %n, 0
  br i1 %1, label %.lr.ph, label %._crit_edge

.lr.ph:                                           ; preds = %0
  %2 = sext i32 %n to i64
  br label %3

3:                                                ; preds = %3, %.lr.ph
  %indvars.iv = phi i64 [ %2, %.lr.ph ], [ %indvars.iv.next, %3 ]
  %indvars.iv.next = add i64 %indvars.iv, -1
  %4 = getelementptr inbounds i32, i32* %b, i64 %indvars.iv.next
  %5 = load i32, i32* %4, align 4
  %6 = add nsw i32 %5, 0
  %7 = trunc i64 %indvars.iv.next to i32
  %8 = icmp sgt i32 %7, 0
  br i1 %8, label %3, label %._crit_edge

._crit_edge:                                      ; preds = %3, %0
  %a.0.lcssa = phi i32 [ 0, %0 ], [ %6, %3 ]
  ret i32 %a.0.lcssa
}


define i32 @tgt(i32* %b, i32 %n) #0 {
  %1 = icmp sgt i32 %n, 0
  br i1 %1, label %.lr.ph, label %._crit_edge

.lr.ph:                                           ; preds = %0
  %2 = sext i32 %n to i64
  %3 = add i32 %n, -1
  %4 = zext i32 %3 to i64
  %5 = add nuw nsw i64 %4, 1
  %min.iters.check = icmp ult i32 %3, 3
  br i1 %min.iters.check, label %scalar.ph, label %vector.ph

vector.ph:                                        ; preds = %.lr.ph
  %n.vec = and i64 %5, 8589934588
  %ind.end = sub nsw i64 %2, %n.vec
  br label %vector.body

vector.body:                                      ; preds = %vector.body, %vector.ph
  %index = phi i64 [ 0, %vector.ph ], [ %index.next, %vector.body ]
  %index.next = add i64 %index, 4
  %6 = icmp eq i64 %index.next, %n.vec
  br i1 %6, label %middle.block, label %vector.body

middle.block:                                     ; preds = %vector.body
  %7 = getelementptr inbounds i32, i32* %b, i64 -3
  %8 = xor i64 %index, -1
  %9 = add i64 %8, %2
  %10 = getelementptr inbounds i32, i32* %7, i64 %9
  %11 = bitcast i32* %10 to <4 x i32>*
  %wide.load = load <4 x i32>, <4 x i32>* %11, align 4
  %cmp.n = icmp eq i64 %5, %n.vec
  %12 = extractelement <4 x i32> %wide.load, i32 0
  br i1 %cmp.n, label %._crit_edge.loopexit, label %scalar.ph

scalar.ph:                                        ; preds = %middle.block, %.lr.ph
  %bc.resume.val = phi i64 [ %ind.end, %middle.block ], [ %2, %.lr.ph ]
  br label %13

13:                                               ; preds = %13, %scalar.ph
  %indvars.iv = phi i64 [ %bc.resume.val, %scalar.ph ], [ %indvars.iv.next, %13 ]
  %indvars.iv.next = add i64 %indvars.iv, -1
  %14 = getelementptr inbounds i32, i32* %b, i64 %indvars.iv.next
  %15 = load i32, i32* %14, align 4
  %16 = trunc i64 %indvars.iv.next to i32
  %17 = icmp sgt i32 %16, 0
  br i1 %17, label %13, label %._crit_edge.loopexit

._crit_edge.loopexit:                             ; preds = %middle.block, %13
  %.lcssa = phi i32 [ %15, %13 ], [ %12, %middle.block ]
  br label %._crit_edge

._crit_edge:                                      ; preds = %._crit_edge.loopexit, %0
  %a.0.lcssa = phi i32 [ 0, %0 ], [ %.lcssa, %._crit_edge.loopexit ]
  ret i32 %a.0.lcssa
}

