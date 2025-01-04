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

declare float @llvm.fabs.f32(float)
