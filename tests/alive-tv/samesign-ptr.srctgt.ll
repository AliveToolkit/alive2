target datalayout = "p:8:8:8"

define i1 @src(ptr %a, ptr %b) {
  %pa = ptrtoint ptr %a to i8
  %pb = ptrtoint ptr %b to i8
  %sub = sub i8 %pb, %pa
  %gep = getelementptr i8, ptr %a, i8 %sub
  %cmp = icmp samesign eq ptr %gep, null
  ret i1 %cmp
}

define i1 @tgt(ptr %a, ptr %b) {
  %cmp = icmp samesign eq ptr %b, null
  ret i1 %cmp
}

define i1 @src_offsetonly() {
  %cmp = icmp samesign eq ptr null, null
  ret i1 %cmp
}

define i1 @tgt_offsetonly() {
  ret i1 true
}
