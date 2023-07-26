define i1 @src(ptr %a, ptr %b) {
  %cond1 = icmp ne ptr %a, %b
  %a2 = getelementptr inbounds i8, ptr %a, i64 -1
  %cond2 = icmp ugt ptr %a2, %b
  %res = select i1 %cond1, i1 %cond2, i1 false
  ret i1 %res
}

define i1 @tgt(ptr %a, ptr %b) {
  %a2 = getelementptr inbounds i8, ptr %a, i64 -1
  %cond2 = icmp ugt ptr %a2, %b
  ret i1 %cond2
}

; ERROR: Target is more poisonous than source
