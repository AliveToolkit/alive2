; a poison lane only affects the matching output lane

define <2 x i32> @src(<4 x i32> %v) {
  %p = insertelement <4 x i32> %v, i32 poison, i32 0
  %d = call { <2 x i32>, <2 x i32> } @llvm.vector.deinterleave2.v4i32(<4 x i32> %p)
  %odd = extractvalue { <2 x i32>, <2 x i32> } %d, 1
  ret <2 x i32> %odd
}

define <2 x i32> @tgt(<4 x i32> %v) {
  %d = call { <2 x i32>, <2 x i32> } @llvm.vector.deinterleave2.v4i32(<4 x i32> %v)
  %odd = extractvalue { <2 x i32>, <2 x i32> } %d, 1
  ret <2 x i32> %odd
}
