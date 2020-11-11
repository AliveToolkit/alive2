; Our old select bug

define i1 @src(i1 %c, i32 %y) {
  %y2 = add nsw i32 %y, 1
  %s = select i1 %c, i32 undef, i32 %y2
  %r = icmp sgt i32 %s, %y
  ret i1 %r
}

define i1 @tgt(i1 %c, i32 %y) {
  ret i1 true
}

; ERROR: Value mismatch
