define i1 @src(i8* %a, i8* %b) {
  %cond1 = icmp ne i8* %a, %b
  %a2 = getelementptr inbounds i8, i8* %a, i64 -1
  %cond2 = icmp ugt i8* %a2, %b
  %res = select i1 %cond1, i1 %cond2, i1 false
  ret i1 %res
}

define i1 @tgt(i8* %a, i8* %b) {
  %a2 = getelementptr inbounds i8, i8* %a, i64 -1
  %cond2 = icmp ugt i8* %a2, %b
  ret i1 %cond2
}

; ERROR: Target is more poisonous than source
