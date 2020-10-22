define float @src(float %x) {
  %y = call float @fabsf(float %x)
  ret float %y
}

define float @tgt(float %x) {
  %y = call float @llvm.fabs.f32(float %x)
  ret float %y
}

declare float @fabsf(float)
declare float @llvm.fabs.f32(float)
