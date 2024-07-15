; TEST-ARGS: --uf-float

define float @src(float noundef %x) {
  %a = fptosi float %x to i32
  %res = sitofp i32 %a to float
  ret float %res
}

define float @tgt(float noundef %x) {
  %vec1 = insertelement <4 x float> poison, float %x, i32 0
  %vec2 = fptosi <4 x float> %vec1 to <4 x i32>
  %vec3 = sitofp <4 x i32> %vec2 to <4 x float>
  %res = extractelement <4 x float> %vec3, i32 0
  ret float %res
}