; ERROR: Value mismatch

; only a signaling NaN element makes the reduction order matter

define double @src() {
  %r = call double @llvm.vector.reduce.fmin.v3f64(<3 x double> <double 0x7FF8000000000000, double 0.0, double 1.0>)
  ret double %r
}

define double @tgt() {
  ret double 1.0
}
