define float @src(float %a) {
  %x = call float @llvm.experimental.constrained.fsub.f32(float %a, float 0.0, metadata !"round.upward", metadata !"fpexcept.strict")
  %y = call float @llvm.experimental.constrained.fsub.f32(float %a, float 0.0, metadata !"round.downward", metadata !"fpexcept.strict")
  %ret = call float @llvm.experimental.constrained.fsub.f32(float %x, float %y, metadata !"round.dynamic", metadata !"fpexcept.strict")
  ret float %ret
}

define float @tgt(float %a) {
  ret float poison
}

declare float @llvm.experimental.constrained.fsub.f32(float, float, metadata, metadata)
