; ERROR: Value mismatch

; the float attribute should not apply to doubles

define double @src() denormal_fpenv(float: positivezero) {
  %result = fdiv double 0x3810000000000000, 2.000000e+00
  ret double %result
}

define double @tgt() denormal_fpenv(float: positivezero) {
  ret double 0.0
}
