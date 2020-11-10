; TEST-ARGS: -src-unroll=8 -tgt-unroll=8
; ERROR: Source is more defined than target

; https://bugs.llvm.org/show_bug.cgi?id=48125

@a = external global [8 x i32], align 16

define void @src() {
entry:
  br label %for.body

for.body:                                         ; preds = %for.body, %entry
  %indvars.iv = phi i64 [ 0, %entry ], [ %indvars.iv.next, %for.body ]
  %arrayidx = getelementptr inbounds [8 x i32], [8 x i32]* @a, i64 0, i64 %indvars.iv
  store i32 undef, i32* %arrayidx, align 4
  %indvars.iv.next = add nuw nsw i64 %indvars.iv, 1
  %exitcond = icmp ne i64 %indvars.iv.next, 8
  br i1 %exitcond, label %for.body, label %for.exit

for.exit:                                         ; preds = %for.body
  ret void
}

define void @tgt() {
entry:
  br label %for.body.peel.begin

for.body.peel.begin:                              ; preds = %entry
  br label %for.body.peel

for.body.peel:                                    ; preds = %for.body.peel.begin
  %arrayidx.peel = getelementptr inbounds [8 x i32], [8 x i32]* @a, i64 0, i64 0
  store i32 undef, i32* %arrayidx.peel, align 4
  %indvars.iv.next.peel = add nuw nsw i64 0, 1
  %exitcond.peel = icmp ne i64 %indvars.iv.next.peel, 8
  br i1 %exitcond.peel, label %for.body.peel.next, label %for.exit

for.body.peel.next:                               ; preds = %for.body.peel
  br label %for.body.peel2

for.body.peel2:                                   ; preds = %for.body.peel.next
  %arrayidx.peel3 = getelementptr inbounds [8 x i32], [8 x i32]* @a, i64 0, i64 %indvars.iv.next.peel
  store i32 undef, i32* %arrayidx.peel3, align 4
  %indvars.iv.next.peel4 = add nuw nsw i64 %indvars.iv.next.peel, 1
  %exitcond.peel5 = icmp ne i64 %indvars.iv.next.peel4, 8
  br i1 %exitcond.peel5, label %for.body.peel.next1, label %for.exit

for.body.peel.next1:                              ; preds = %for.body.peel2
  br label %for.body.peel.next6

for.body.peel.next6:                              ; preds = %for.body.peel.next1
  br label %entry.peel.newph

entry.peel.newph:                                 ; preds = %for.body.peel.next6
  br label %for.body

for.body:                                         ; preds = %entry.peel.newph
  %arrayidx = getelementptr inbounds [8 x i32], [8 x i32]* @a, i64 0, i64 %indvars.iv.next.peel4
  store i32 undef, i32* %arrayidx, align 4
  store i32 undef, i32* getelementptr inbounds ([8 x i32], [8 x i32]* @a, i64 0, i64 3), align 4
  store i32 undef, i32* getelementptr inbounds ([8 x i32], [8 x i32]* @a, i64 0, i64 4), align 4
  store i32 undef, i32* getelementptr inbounds ([8 x i32], [8 x i32]* @a, i64 0, i64 5), align 4
  store i32 undef, i32* getelementptr inbounds ([8 x i32], [8 x i32]* @a, i64 0, i64 6), align 4
  store i32 undef, i32* getelementptr inbounds ([8 x i32], [8 x i32]* @a, i64 0, i64 7), align 4
  store i32 undef, i32* getelementptr inbounds ([8 x i32], [8 x i32]* @a, i64 1, i64 0), align 4
  store i32 undef, i32* getelementptr ([8 x i32], [8 x i32]* @a, i64 1, i64 1), align 4
  br label %for.exit

for.exit:                                         ; preds = %for.body, %for.body.peel2, %for.body.peel
  ret void
}
