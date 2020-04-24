define i1 @src(i8* %a, i8* %b) {
  %ia = ptrtoint i8* %a to i64
  %ib = ptrtoint i8* %b to i64
  %c = icmp eq i64 %ia, %ib
  ret i1 %c
}

define i1 @tgt(i8* %a, i8* %b) {
  %c = icmp eq i8* %a, %b
  ret i1 %c
}
