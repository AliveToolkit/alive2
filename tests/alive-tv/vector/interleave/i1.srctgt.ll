define <8 x i1> @src(<4 x i1> %a, <4 x i1> %b) {
  %r = call <8 x i1> @llvm.vector.interleave2.v8i1(<4 x i1> %a, <4 x i1> %b)
  ret <8 x i1> %r
}

define <8 x i1> @tgt(<4 x i1> %a, <4 x i1> %b) {
  %r = shufflevector <4 x i1> %a, <4 x i1> %b, <8 x i32> <i32 0, i32 4, i32 1, i32 5, i32 2, i32 6, i32 3, i32 7>
  ret <8 x i1> %r
}
