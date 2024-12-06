; TEST-ARGS: --uf-float

define float @src(float noundef %x, float noundef %y) {
  %sum = fadd float %x, %y
  ret float %sum
}

define float @tgt(float noundef %x, float noundef %y) {
  %sum = fadd float %y, %x
  ret float %sum
}