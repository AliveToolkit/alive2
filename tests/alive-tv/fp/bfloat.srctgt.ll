define i16 @src() {
  %r = bitcast bfloat 3.0 to i16
  ret i16 %r
}

define i16 @tgt() {
  ret i16 16448
}
