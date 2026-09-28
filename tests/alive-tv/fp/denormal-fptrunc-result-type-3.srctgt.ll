; ERROR: Value mismatch

; As denormal-fptrunc-result-type-2.srctgt.ll, but "float: ieee" overrides the
; base positivezero for the f32 result type, so the result is not flushed.

define float @src() denormal_fpenv(positivezero, float: ieee) {
; 0x3730000000000000 is 0x1p-140: a normal double, and a subnormal float
  %v = fptrunc double 0x3730000000000000 to float
  ret float %v
}

define float @tgt() denormal_fpenv(positivezero, float: ieee) {
  ret float 0.000000e+00
}
