; Reported by https://reviews.llvm.org/D90529#2619492

define i32 @src(i32* nocapture %a) nounwind uwtable readonly {
entry:
  tail call void @llvm.assume(i1 true) ["align"(i32* %a, i32 32, i32 28)]
  %arrayidx = getelementptr inbounds i32, i32* %a, i64 -1
  %.0 = load i32, i32* %arrayidx, align 4
  ret i32 %.0
}

define i32 @tgt(i32* nocapture %a) nounwind uwtable readonly {
entry:
  tail call void @llvm.assume(i1 true) ["align"(i32* %a, i32 32, i32 28)]
  %arrayidx = getelementptr inbounds i32, i32* %a, i64 -1
  %.0 = load i32, i32* %arrayidx, align 32
  ret i32 %.0
}

declare void @llvm.assume(i1)

; ERROR: Source is more defined than target
