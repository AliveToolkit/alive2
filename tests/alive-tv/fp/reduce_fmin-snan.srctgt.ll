; the reduction order of reduce.fmin/fmax is non-deterministic, so a signaling
; NaN element lets any element, or a NaN, be returned.

define double @src_lane1() {
  %r = call double @llvm.vector.reduce.fmin.v3f64(<3 x double> <double 0x7FF0000000000001, double 0.0, double 1.0>)
  ret double %r
}

define double @tgt_lane1() {
  ret double 0.0
}

define double @src_lane2() {
  %r = call double @llvm.vector.reduce.fmin.v3f64(<3 x double> <double 0x7FF0000000000001, double 0.0, double 1.0>)
  ret double %r
}

define double @tgt_lane2() {
  ret double 1.0
}

define double @src_qnan() {
  %r = call double @llvm.vector.reduce.fmin.v3f64(<3 x double> <double 0x7FF0000000000001, double 0.0, double 1.0>)
  ret double %r
}

define double @tgt_qnan() {
  ret double 0x7FF8000000000000
}

define double @src_snan() {
  %r = call double @llvm.vector.reduce.fmin.v3f64(<3 x double> <double 0x7FF0000000000001, double 0.0, double 1.0>)
  ret double %r
}

define double @tgt_snan() {
  ret double 0x7FF0000000000001
}

; sign bit of NaN result is non-deterministic

define double @src_neg_qnan() {
  %r = call double @llvm.vector.reduce.fmin.v3f64(<3 x double> <double 0x7FF0000000000001, double 0.0, double 1.0>)
  ret double %r
}

define double @tgt_neg_qnan() {
  ret double 0xFFF8000000000000
}

define double @src_neg_snan() {
  %r = call double @llvm.vector.reduce.fmin.v3f64(<3 x double> <double 0x7FF0000000000001, double 0.0, double 1.0>)
  ret double %r
}

define double @tgt_neg_snan() {
  ret double 0xFFF0000000000001
}
