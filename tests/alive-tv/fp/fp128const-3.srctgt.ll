define fp128 @src() {
  %r = fpext float 9223372036854775808.0 to fp128
  ret fp128 %r
}

define fp128 @tgt() {
  ret fp128 0xL0000000000000000403E000000000000
}
