define <4 x i32> @src(<8 x i32> %v) {
  %d = call { <4 x i32>, <4 x i32> } @llvm.vector.deinterleave2.v8i32(<8 x i32> %v)
  %odd = extractvalue { <4 x i32>, <4 x i32> } %d, 1
  ret <4 x i32> %odd
}

define <4 x i32> @tgt(<8 x i32> %v) {
  %odd = shufflevector <8 x i32> %v, <8 x i32> poison, <4 x i32> <i32 1, i32 3, i32 5, i32 7>
  ret <4 x i32> %odd
}
