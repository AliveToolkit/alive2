define double @src() denormal_fpenv(positivezero) {
  %result = fneg double 0x8000000000000
  ret double %result
}

define double @tgt() denormal_fpenv(positivezero) {
  ret double 0x8008000000000000
}
