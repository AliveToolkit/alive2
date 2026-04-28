define float @src() denormal_fpenv(float: positivezero) {
  %result = fdiv float 0x3810000000000000, 2.000000e+00
  ret float %result
}

define float @tgt() denormal_fpenv(float: positivezero) {
  ret float 0.0
}
