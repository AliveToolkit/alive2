; <3 x i8> has a 4-byte alloc size, so the result struct has padding
; between its fields; field 1 must not be confused with the padding

define <3 x i8> @src(<6 x i8> %v) {
  %d = call { <3 x i8>, <3 x i8> } @llvm.vector.deinterleave2.v6i8(<6 x i8> %v)
  %odd = extractvalue { <3 x i8>, <3 x i8> } %d, 1
  ret <3 x i8> %odd
}

define <3 x i8> @tgt(<6 x i8> %v) {
  %odd = shufflevector <6 x i8> %v, <6 x i8> poison, <3 x i32> <i32 1, i32 3, i32 5>
  ret <3 x i8> %odd
}
