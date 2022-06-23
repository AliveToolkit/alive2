define double @src() "denormal-fp-math"="positive-zero" {
  %result = fneg double 0x8000000000000
  ret double %result
}

define double @tgt() "denormal-fp-math"="positive-zero" {
  ret double 0x8008000000000000
}
