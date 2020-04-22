define i256 @src() {
  %r1 = lshr i256 2, 18446744073709551617
  ret i256 %r1
}

define i256 @tgt() {
  ret i256 undef
}
