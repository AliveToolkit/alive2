define float @src(float %a) {
  %fabs = call float @llvm.fabs.f32(float noundef %a)
  ret float %fabs
}

define noundef float @tgt(float %a) {
  %fabs = call float @llvm.fabs.f32(float noundef %a)
  ret float %fabs
}

declare float @llvm.fabs.f32(float)
