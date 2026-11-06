; nsz: either operand may be returned for two zeros of opposite sign

define double @src_nsz_pos() {
  %r = call nsz double @llvm.minnum.f64(double 0.0, double -0.0)
  ret double %r
}

define double @tgt_nsz_pos() {
  ret double 0.0
}

define double @src_nsz_neg() {
  %r = call nsz double @llvm.maximum.f64(double 0.0, double -0.0)
  ret double %r
}

define double @tgt_nsz_neg() {
  ret double -0.0
}

; minnum/maxnum either quiet a signaling NaN operand or return a NaN

define double @src_snan_nan() {
  %r = call double @llvm.minnum.f64(double 0x7FF0000000000001, double 1.0)
  ret double %r
}

define double @tgt_snan_nan() {
  ret double 0x7FF8000000000000
}

define double @src_snan_quiet() {
  %r = call double @llvm.maxnum.f64(double 0x7FF0000000000001, double 1.0)
  ret double %r
}

define double @tgt_snan_quiet() {
  ret double 1.0
}

; minnum can be refined into minimumnum

define float @src_refine(float %x, float %y) {
  %r = call float @llvm.minnum.f32(float %x, float %y)
  ret float %r
}

define float @tgt_refine(float %x, float %y) {
  %r = call float @llvm.minimumnum.f32(float %x, float %y)
  ret float %r
}
