; TEST-ARGS: --uf-float

define float @src(float noundef %x, float noundef %y) {
  %sum = fadd float %x, %y
  ret float %sum
}

define float @tgt(float noundef %x, float noundef %y) {
  %vec1 = insertelement <2 x float> poison, float %x, i32 0
  %vec2 = insertelement <2 x float> poison, float %y, i32 0
  %vec3 = fadd <2 x float> %vec1, %vec2
  %sum = extractelement <2 x float> %vec3, i32 0
  ret float %sum
}