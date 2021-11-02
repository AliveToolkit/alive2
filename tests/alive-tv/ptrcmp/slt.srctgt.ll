define i1 @src(i8* %ptr) {
entry:
  %first1 = getelementptr inbounds i8, i8* %ptr, i32 0
  %last1 = getelementptr inbounds i8, i8* %ptr, i32 42
  %cmp = icmp slt i8* %first1, %last1
  ret i1 %cmp
}

define i1 @tgt(i8* %ptr) {
entry:
  %first1 = getelementptr inbounds i8, i8* %ptr, i32 0
  %last1 = getelementptr i8, i8* %ptr, i32 42
  %cmp = icmp slt i8* %first1, %last1
  ret i1 %cmp
}


