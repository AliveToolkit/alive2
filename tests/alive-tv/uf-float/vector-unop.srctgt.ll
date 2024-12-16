; TEST-ARGS: --uf-float

define float @src(float noundef %x) {
  %res = fneg float %x
  ret float %res
}

define float @tgt(float noundef %x) {
  %vec1 = insertelement <2 x float> poison, float %x, i32 0
  %vec2 = fneg <2 x float> %vec1
  %res = extractelement <2 x float> %vec2, i32 0
  ret float %res
}