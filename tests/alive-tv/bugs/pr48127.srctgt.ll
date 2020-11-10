; TEST-ARGS: -src-unroll=4 -tgt-unroll=4
; ERROR: Source is more defined than target

; https://bugs.llvm.org/show_bug.cgi?id=48127

define void @src(i32* %a, i32 %x, i32 %y, i32 %z, i64 %n) {
entry:
  br label %for.body

for.body:                                         ; preds = %for.body, %entry
  %i = phi i64 [ %i.next, %for.body ], [ 0, %entry ]
  %i_plus_1 = add i64 %i, 1
  %a_i = getelementptr inbounds i32, i32* %a, i64 %i
  %a_i_plus_1 = getelementptr inbounds i32, i32* %a, i64 %i_plus_1
  store i32 %x, i32* %a_i, align 4
  store i32 %y, i32* %a_i, align 4
  store i32 %z, i32* %a_i_plus_1, align 4
  %i.next = add nuw nsw i64 %i, 2
  %cond = icmp slt i64 %i.next, %n
  br i1 %cond, label %for.body, label %for.end

for.end:                                          ; preds = %for.body
  ret void
}


define void @tgt(i32* %a, i32 %x, i32 %y, i32 %z, i64 %n) {
entry:
  %0 = icmp sgt i64 %n, 2
  %smax = select i1 %0, i64 %n, i64 2
  %1 = add nsw i64 %smax, -1
  %2 = lshr i64 %1, 1
  %3 = add nuw nsw i64 %2, 1
  %min.iters.check = icmp ult i64 %1, 6
  br i1 %min.iters.check, label %scalar.ph, label %vector.ph

vector.ph:                                        ; preds = %entry
  %n.vec = and i64 %3, 9223372036854775804
  %ind.end = shl nuw i64 %n.vec, 1
  %broadcast.splatinsert = insertelement <4 x i32> undef, i32 %y, i32 0
  %broadcast.splat = shufflevector <4 x i32> %broadcast.splatinsert, <4 x i32> undef, <4 x i32> zeroinitializer
  %broadcast.splatinsert1 = insertelement <4 x i32> undef, i32 %z, i32 0
  %broadcast.splat2 = shufflevector <4 x i32> %broadcast.splatinsert1, <4 x i32> undef, <4 x i32> zeroinitializer
  br label %vector.body

vector.body:                                      ; preds = %vector.body, %vector.ph
  %index = phi i64 [ 0, %vector.ph ], [ %index.next, %vector.body ]
  %offset.idx = shl i64 %index, 1
  %4 = or i64 %offset.idx, 2
  %5 = or i64 %offset.idx, 4
  %6 = or i64 %offset.idx, 6
  %7 = or i64 %offset.idx, 1
  %8 = getelementptr inbounds i32, i32* %a, i64 %offset.idx
  %9 = getelementptr inbounds i32, i32* %a, i64 %4
  %10 = getelementptr inbounds i32, i32* %a, i64 %5
  %11 = getelementptr inbounds i32, i32* %a, i64 %6
  %12 = getelementptr inbounds i32, i32* %a, i64 -1
  store i32 %x, i32* %8, align 4
  store i32 %x, i32* %9, align 4
  store i32 %x, i32* %10, align 4
  store i32 %x, i32* %11, align 4
  %13 = getelementptr inbounds i32, i32* %12, i64 %7
  %14 = bitcast i32* %13 to <8 x i32>*
  %interleaved.vec = shufflevector <4 x i32> %broadcast.splat, <4 x i32> %broadcast.splat2, <8 x i32> <i32 0, i32 4, i32 1, i32 5, i32 2, i32 6, i32 3, i32 7>
  store <8 x i32> %interleaved.vec, <8 x i32>* %14, align 4
  %index.next = add i64 %index, 4
  %15 = icmp eq i64 %index.next, %n.vec
  br i1 %15, label %middle.block, label %vector.body

middle.block:                                     ; preds = %vector.body
  %cmp.n = icmp eq i64 %3, %n.vec
  br i1 %cmp.n, label %for.end, label %scalar.ph

scalar.ph:                                        ; preds = %middle.block, %entry
  %bc.resume.val = phi i64 [ %ind.end, %middle.block ], [ 0, %entry ]
  br label %for.body

for.body:                                         ; preds = %for.body, %scalar.ph
  %i = phi i64 [ %i.next, %for.body ], [ %bc.resume.val, %scalar.ph ]
  %i_plus_1 = or i64 %i, 1
  %a_i = getelementptr inbounds i32, i32* %a, i64 %i
  %a_i_plus_1 = getelementptr inbounds i32, i32* %a, i64 %i_plus_1
  store i32 %y, i32* %a_i, align 4
  store i32 %z, i32* %a_i_plus_1, align 4
  %i.next = add nuw nsw i64 %i, 2
  %cond = icmp slt i64 %i.next, %n
  br i1 %cond, label %for.body, label %for.end

for.end:                                          ; preds = %middle.block, %for.body
  ret void
}

