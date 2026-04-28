define double @src() denormal_fpenv(positivezero) {
  %result = fadd double 0x8000000000000, 0.0
  ret double %result
}

define double @tgt() denormal_fpenv(positivezero) {
  ret double 0.0
}
