; LangRef, denormal_fpenv: "If the input mode is preservesign, or
; positivezero, a floating-point operation must treat any input denormal
; value as zero."  Unlike output flushing, input flushing is mandatory, so
; the fdiv below definitely sees +0.0 and definitely returns +inf.

define float @src() denormal_fpenv(ieee|positivezero) {
  ret float 0x7FF0000000000000
}

define float @tgt() denormal_fpenv(ieee|positivezero) {
; 0xB800000000000000 is -0x1p-127, a negative subnormal float
  %v = fdiv float 1.000000e+00, 0xB800000000000000
  ret float %v
}
