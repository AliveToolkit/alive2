; https://bugs.llvm.org/show_bug.cgi?id=39510

define i1 @src(i32 %a) {
  %cmp = icmp slt i32 %a, 0
  %sub = sub nsw i32 0, %a
  %cond = select i1 %cmp, i32 %sub, i32 %a
  %r = icmp ne i32 %cond, 2
  ret i1 %r
}

define i1 @tgt(i32 %a) {
	ret i1 1
}

; ERROR: Value mismatch
