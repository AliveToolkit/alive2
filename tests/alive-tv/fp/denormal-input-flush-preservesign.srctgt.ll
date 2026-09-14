; As denormal-input-flush.srctgt.ll, but preservesign keeps the sign of the
; flushed input, so the result is -inf rather than +inf.

define float @src() denormal_fpenv(ieee|preservesign) {
  ret float 0xFFF0000000000000
}

define float @tgt() denormal_fpenv(ieee|preservesign) {
; 0xB800000000000000 is -0x1p-127, a negative subnormal float
  %v = fdiv float 1.000000e+00, 0xB800000000000000
  ret float %v
}
