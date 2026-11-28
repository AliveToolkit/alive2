; ERROR: Value mismatch
; CHECK-NOT: Transformation seems to be correct!

; -0.0 compares less than +0.0, deterministically without nsz

define double @src_zero_min() {
  %r = call double @llvm.minnum.f64(double 0.0, double -0.0)
  ret double %r
}

define double @tgt_zero_min() {
  ret double 0.0
}

define double @src_zero_max() {
  %r = call double @llvm.maxnum.f64(double 0.0, double -0.0)
  ret double %r
}

define double @tgt_zero_max() {
  ret double -0.0
}

; nsz doesn't change the sign of the result if both operands have the same sign

define double @src_nsz_num() {
  %r = call nsz double @llvm.minnum.f64(double 0.0, double 0.0)
  ret double %r
}

define double @tgt_nsz_num() {
  ret double -0.0
}

define double @src_nsz_imum() {
  %r = call nsz double @llvm.minimum.f64(double 0.0, double 0.0)
  ret double %r
}

define double @tgt_nsz_imum() {
  ret double -0.0
}

; only a signaling NaN operand lets minnum/maxnum return a NaN

define double @src_qnan() {
  %r = call double @llvm.minnum.f64(double 0x7FF8000000000000, double 1.0)
  ret double %r
}

define double @tgt_qnan() {
  ret double 0x7FF8000000000000
}

; minimumnum/maximumnum return the number, even for a signaling NaN operand

define double @src_snan() {
  %r = call double @llvm.minimumnum.f64(double 0x7FF0000000000001, double 1.0)
  ret double %r
}

define double @tgt_snan() {
  ret double 0x7FF8000000000000
}

; a signaling NaN result is only reachable by copying an input NaN unchanged

define double @src_snan_payload() {
  %r = call double @llvm.minnum.f64(double 0x7FF0000000000001, double 1.0)
  ret double %r
}

define double @tgt_snan_payload() {
  ret double 0x7FF0000000000002
}
