; ERROR: Target is more poisonous than source

define <2 x i32> @src(<2 x i32> %a, <2 x i32> %b) {
  %r = call <4 x i32> @llvm.vector.interleave2.v4i32(<2 x i32> %a, <2 x i32> %b)
  %e = shufflevector <4 x i32> %r, <4 x i32> poison, <2 x i32> <i32 1, i32 3>
  ret <2 x i32> %e
}

define <2 x i32> @tgt(<2 x i32> %a, <2 x i32> %b) {
  %pb = insertelement <2 x i32> %b, i32 poison, i32 1
  ret <2 x i32> %pb
}
