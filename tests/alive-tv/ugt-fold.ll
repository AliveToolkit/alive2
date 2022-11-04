; ERROR: Value mismatch

define i1 @src(i72 %L)  {
  %ugt = icmp ugt i72 %L, u0xffffffffffffffff 
  ret i1 %ugt
}

define i1 @tgt(i72 %L) {
  ret i1 1
}
