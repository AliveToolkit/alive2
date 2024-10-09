; TEST-ARGS: --uf-float

define i1 @src(float noundef %x, float noundef %y) {
  %res = fcmp olt float %x, %y
  ret i1 %res
}

define i1 @tgt(float noundef %x, float noundef %y) {
  %vec1 = insertelement <2 x float> poison, float %x, i32 0
  %vec2 = insertelement <2 x float> poison, float %y, i32 0
  %vec3 = fcmp olt <2 x float> %vec1, %vec2
  %res = extractelement <2 x i1> %vec3, i32 0
  ret i1 %res
}