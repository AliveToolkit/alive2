define <2 x i1> @src(<2 x float> %x) {
entry:
  %0 = tail call <2 x i1> @llvm.is.fpclass.v2f32(<2 x float> <float -0.0, float 1.0>, i32 256) ; is pos
  ret <2 x i1> %0
}

define <2 x i1> @tgt(<2 x float> %x) {
  ret <2 x i1> <i1 false, i1 true>
}

declare <2 x i1> @llvm.is.fpclass.v2f32(<2 x float>, i32)
