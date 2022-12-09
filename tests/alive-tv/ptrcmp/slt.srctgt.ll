define i1 @src(ptr %ptr) {
entry:
  %first1 = getelementptr inbounds i8, ptr %ptr, i32 0
  %last1 = getelementptr inbounds i8, ptr %ptr, i32 42
  %cmp = icmp slt ptr %first1, %last1
  ret i1 %cmp
}

define i1 @tgt(ptr %ptr) {
entry:
  %first1 = getelementptr inbounds i8, ptr %ptr, i32 0
  %last1 = getelementptr i8, ptr %ptr, i32 42
  %cmp = icmp slt ptr %first1, %last1
  ret i1 %cmp
}
