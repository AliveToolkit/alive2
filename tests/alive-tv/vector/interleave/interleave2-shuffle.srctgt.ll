define <8 x i16> @src(<4 x i16> %a, <4 x i16> %b) {
  %r = call <8 x i16> @llvm.vector.interleave2.v8i16(<4 x i16> %a, <4 x i16> %b)
  ret <8 x i16> %r
}

define <8 x i16> @tgt(<4 x i16> %a, <4 x i16> %b) {
  %r = shufflevector <4 x i16> %a, <4 x i16> %b, <8 x i32> <i32 0, i32 4, i32 1, i32 5, i32 2, i32 6, i32 3, i32 7>
  ret <8 x i16> %r
}
