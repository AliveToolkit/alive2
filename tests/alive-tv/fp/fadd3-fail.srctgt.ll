; ERROR: Target is more poisonous than source

define float @src(float noundef %a) {
  %x = call float @llvm.experimental.constrained.fsub.f32(float %a, float 0.0, metadata !"round.upward", metadata !"fpexcept.ignore")
  %y = call float @llvm.experimental.constrained.fsub.f32(float %a, float 0.0, metadata !"round.upward", metadata !"fpexcept.ignore")
  %ret = call float @llvm.experimental.constrained.fsub.f32(float %x, float %y, metadata !"round.dynamic", metadata !"fpexcept.ignore")
  ret float %ret
}

define float @tgt(float noundef %a) {
  ret float poison
}

declare float @llvm.experimental.constrained.fsub.f32(float, float, metadata, metadata)
