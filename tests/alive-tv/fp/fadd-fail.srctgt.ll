; ERROR: Value mismatch

define float @src(float noundef %a) {
  %ret = call float @llvm.experimental.constrained.fsub.f32(float %a, float 0.0, metadata !"round.downward", metadata !"fpexcept.strict")
  ret float %ret
}

define float @tgt(float noundef %a) {
  ret float %a
}

declare float @llvm.experimental.constrained.fsub.f32(float, float, metadata, metadata)
