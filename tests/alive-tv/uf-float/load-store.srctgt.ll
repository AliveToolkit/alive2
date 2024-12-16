; TEST-ARGS: --uf-float

define float @src(float noundef %x) {
  ret float %x
}

define float @tgt(float noundef %x) {
  %a = alloca float
  store float %x, ptr %a
  %b = load float, ptr %a
  ret float %b
}