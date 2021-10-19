; ERROR: Value mismatch

define fp128 @src() {
  %r = fpext float 1.5 to fp128
  ret fp128 %r
}

define fp128 @tgt() {
  ret fp128 0xL00000000000000008FFF800000000000
}
