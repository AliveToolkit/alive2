; Reported by https://reviews.llvm.org/D90529#2619492

define i32 @src(ptr nocapture %a) nounwind memory(read) {
entry:
  tail call void @llvm.assume(i1 true) ["align"(ptr %a, i32 32, i32 28)]
  %arrayidx = getelementptr inbounds i32, ptr %a, i64 -1
  %.0 = load i32, ptr %arrayidx, align 4
  ret i32 %.0
}

define i32 @tgt(ptr nocapture %a) nounwind memory(read) {
entry:
  tail call void @llvm.assume(i1 true) ["align"(ptr %a, i32 32, i32 28)]
  %arrayidx = getelementptr inbounds i32, ptr %a, i64 -1
  %.0 = load i32, ptr %arrayidx, align 32
  ret i32 %.0
}

declare void @llvm.assume(i1)

; ERROR: Source is more defined than target
