define double @src() "denormal-fp-math"="positive-zero" {
  %result = fadd double 0x8000000000000, 0.0
  ret double %result
}

define double @tgt() "denormal-fp-math"="positive-zero" {
  ret double 0.0
}
