define float @src() {
  %nan = fdiv float 0.0, 0.0

  %f1 = call nnan float @llvm.fabs.f32(float %nan)
  ret float %f1
}

define float @tgt() {
  ret float undef
}


declare float @llvm.fabs.f32(float)
