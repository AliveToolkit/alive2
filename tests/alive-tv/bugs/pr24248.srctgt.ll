; https://bugs.llvm.org/show_bug.cgi?id=24248 , with minor modification (to make %pos visible)

define void @src(i32 %x) {
entry:
  %vecinit = insertelement <2 x i32> <i32 0, i32 0>, i32 %x, i32 1
  %inc = add i32 %x, 1
  %0 = insertelement <2 x i32> %vecinit, i32 %inc, i32 1
  br label %loop

loop:                                        ; preds = %loop, %entry
  %pos = phi <2 x i32> [ %0, %entry ], [ %new.pos.y, %loop ]
  %i = phi i32 [ 0, %entry ], [ %new.i, %loop ]
  call void @f(<2 x i32> %pos)
  %pos.y = extractelement <2 x i32> %pos, i32 1
  %inc.pos.y = add i32 %pos.y, 1
  %new.pos.y = insertelement <2 x i32> %pos, i32 %inc.pos.y, i32 1
  %new.i = add i32 %i, 1
  %cmp2 = icmp slt i32 %new.i, 1
  br i1 %cmp2, label %loop, label %exit

exit:                                        ; preds = %loop
  ret void
}

define void @tgt(i32 %x) {
entry:
  %vecinit = insertelement <2 x i32> zeroinitializer, i32 %x, i32 1
  %inc = add i32 %x, 1
  %0 = insertelement <2 x i32> %vecinit, i32 %inc, i32 1
  br label %loop

loop:                                             ; preds = %loop, %entry
  %pos.i0 = phi i32 [ 0, %entry ], [ %pos.i01, %loop ]
  %pos.i1 = phi i32 [ %x, %entry ], [ %inc.pos.y, %loop ]
  %i = phi i32 [ 0, %entry ], [ %new.i, %loop ]
  %pos.upto0 = insertelement <2 x i32> undef, i32 %pos.i0, i32 0
  %pos = insertelement <2 x i32> %pos.upto0, i32 %pos.i1, i32 1
  call void @f(<2 x i32> %pos)
  %pos.y = extractelement <2 x i32> %pos, i32 1
  %inc.pos.y = add i32 %pos.y, 1
  %new.pos.y = insertelement <2 x i32> %pos, i32 %inc.pos.y, i32 1
  %pos.i01 = extractelement <2 x i32> %pos, i32 0
  %new.i = add i32 %i, 1
  %cmp2 = icmp slt i32 %new.i, 1
  br i1 %cmp2, label %loop, label %exit

exit:                                             ; preds = %loop
  ret void
}

declare void @f(<2 x i32>)

; ERROR: Source is more defined than target
