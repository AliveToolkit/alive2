define <2 x float> @src(<2 x float> %a, <2 x float> %b, <2 x float> %c) {
  %i = call <6 x float> @llvm.vector.interleave3.v6f32(<2 x float> %a, <2 x float> %b, <2 x float> %c)
  %d = call { <2 x float>, <2 x float>, <2 x float> } @llvm.vector.deinterleave3.v6f32(<6 x float> %i)
  %r = extractvalue { <2 x float>, <2 x float>, <2 x float> } %d, 1
  ret <2 x float> %r
}

define <2 x float> @tgt(<2 x float> %a, <2 x float> %b, <2 x float> %c) {
  ret <2 x float> %b
}
