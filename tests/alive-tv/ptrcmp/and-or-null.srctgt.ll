define i1 @src(i8* %a, i8* %b) {
  %c1 = icmp ule i8* %a, %b
  %c2 = icmp eq i8* %a, null
  %c = or i1 %c1, %c2
  ret i1 %c
}

define i1 @tgt(i8* %a, i8* %b) {
  %c1 = icmp ule i8* %a, %b
  ret i1 %c1
}
