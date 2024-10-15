define i1 @src(i8 %x, i8 %y) {
  %cmp = icmp samesign slt i8 %x, %y
  ret i1 %cmp
}

define i1 @tgt(i8 %x, i8 %y) {
  %cmp = icmp samesign ult i8 %x, %y
  ret i1 %cmp
}
