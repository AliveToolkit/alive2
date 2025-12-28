declare float @llvm.vector.reduce.fmax.v2f32(<2 x float>)

define float @src_fmax(<2 x float> %x) {
  %v0 = extractelement <2 x float> %x, i32 0
  %v1 = extractelement <2 x float> %x, i32 1
  ; ordered greater-than (false if NaN)
  %cmp = fcmp ogt float %v0, %v1
  %r = select i1 %cmp, float %v0, float %v1
  ret float %r
}

define float @tgt_fmax(<2 x float> %x) {
  %r = call float @llvm.vector.reduce.fmax.v2f32(<2 x float> %x)
  ret float %r
}

