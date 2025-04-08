define float @src() {
  %nan = fdiv float 0.0, 0.0

  %f1 = call nnan float @llvm.fabs.f32(float %nan)
  ret float %f1
}

define float @tgt() {
  ret float undef
}

; Ignore sign difference between two NaNs
define half @src1(half noundef %x) {
  %gtzero = fcmp ugt half %x, 0xH8000
  %negx = fsub nnan ninf half 0xH0000, %x
  %fabs = select ninf i1 %gtzero, half %x, half %negx
  ret half %fabs
}

define half @tgt1(half noundef %x) {
  %fabs = call ninf half @llvm.fabs(half %x)
  ret half %fabs
}

define i16 @src2(half noundef %x) {
  %gtzero = fcmp ugt half %x, 0xH8000
  %negx = fsub nnan ninf half 0xH0000, %x
  %fabs = select ninf i1 %gtzero, half %x, half %negx
  %cast = bitcast half %fabs to i16
  ret i16 %cast
}

define i16 @tgt2(half noundef %x) {
  %fabs = call ninf half @llvm.fabs(half %x)
  %cast = bitcast half %fabs to i16
  ret i16 %cast
}

declare float @llvm.fabs.f32(float)
