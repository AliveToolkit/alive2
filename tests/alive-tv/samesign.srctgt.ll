define i1 @src(i8 %x, i8 %y) {
  %cmp = icmp samesign slt i8 %x, %y
  ret i1 %cmp
}

define i1 @tgt(i8 %x, i8 %y) {
  %cmp = icmp samesign ult i8 %x, %y
  ret i1 %cmp
}

define i1 @src1(i64 %x) {
  %ashr = ashr i64 %x, -1
  %cmp = icmp samesign ugt i64 %ashr, 0
  ret i1 %cmp
}

define i1 @tgt1(i64 %x) {
  ret i1 poison
}
