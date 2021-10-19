define fp128 @src() {
  %r = fpext float -1.5 to fp128
  ret fp128 %r
}

define fp128 @tgt() {
  ret fp128 0xL0000000000000000BFFF800000000000
}
