define float @src(float %a) {
  %ret = call float @llvm.experimental.constrained.fsub.f32(float %a, float 0.0, metadata !"round.upward", metadata !"fpexcept.strict")
  ret float %ret
}

define float @tgt(float %a) {
  ret float %a
}

declare float @llvm.experimental.constrained.fsub.f32(float, float, metadata, metadata)
