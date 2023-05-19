; TEST-ARGS: -src-unroll=2 -tgt-unroll=2
; llvm/llvm/test/Transforms/LoopSimplifyCFG/constant-fold-branch.ll

define i32 @src(i32 %end) {
entry:
  br label %outer_header

outer_header:
  %j = phi i32 [0, %entry], [%j.inc, %outer_backedge]
  br label %preheader

preheader:
  br label %header

header:
  %i = phi i32 [0, %preheader], [%i.inc, %backedge]
  br label %backedge

backedge:
  %i.inc = add i32 %i, 1
  %cmp = icmp slt i32 %i.inc, %end
  br i1 %cmp, label %header, label %outer_backedge

outer_backedge:
  %j.inc = add i32 %j, 1
  %cmp.j = icmp slt i32 %j.inc, %end
  br i1 %cmp.j, label %outer_header, label %exit

exit:
  ret i32 %i.inc
}

define i32 @tgt(i32 %end) {
entry:
  br label %outer_header

outer_header:                                     ; preds = %outer_backedge, %entry
  %j = phi i32 [ 0, %entry ], [ %j.inc, %outer_backedge ]
  br label %preheader

preheader:                                        ; preds = %outer_header
  br label %header

header:                                           ; preds = %backedge, %preheader
  %i = phi i32 [ 0, %preheader ], [ %i.inc, %backedge ]
  br label %backedge

backedge:                                         ; preds = %header
  %i.inc = add i32 %i, 1
  %cmp = icmp slt i32 %i.inc, %end
  br i1 %cmp, label %header, label %outer_backedge

outer_backedge:                                   ; preds = %backedge
  %i.inc.lcssa = phi i32 [ %i.inc, %backedge ]
  %j.inc = add i32 %j, 1
  %cmp.j = icmp slt i32 %j.inc, %end
  br i1 %cmp.j, label %outer_header, label %exit

exit:                                             ; preds = %outer_backedge
  %i.inc.lcssa.lcssa = phi i32 [ %i.inc.lcssa, %outer_backedge ]
  ret i32 %i.inc.lcssa.lcssa
}
