define <16 x i8> @src(<16 x i8> noundef %v) {
  %d = call { <2 x i8>, <2 x i8>, <2 x i8>, <2 x i8>, <2 x i8>, <2 x i8>, <2 x i8>, <2 x i8> } @llvm.vector.deinterleave8.v16i8(<16 x i8> noundef %v)
  %d0 = extractvalue { <2 x i8>, <2 x i8>, <2 x i8>, <2 x i8>, <2 x i8>, <2 x i8>, <2 x i8>, <2 x i8> } %d, 0
  %d1 = extractvalue { <2 x i8>, <2 x i8>, <2 x i8>, <2 x i8>, <2 x i8>, <2 x i8>, <2 x i8>, <2 x i8> } %d, 1
  %d2 = extractvalue { <2 x i8>, <2 x i8>, <2 x i8>, <2 x i8>, <2 x i8>, <2 x i8>, <2 x i8>, <2 x i8> } %d, 2
  %d3 = extractvalue { <2 x i8>, <2 x i8>, <2 x i8>, <2 x i8>, <2 x i8>, <2 x i8>, <2 x i8>, <2 x i8> } %d, 3
  %d4 = extractvalue { <2 x i8>, <2 x i8>, <2 x i8>, <2 x i8>, <2 x i8>, <2 x i8>, <2 x i8>, <2 x i8> } %d, 4
  %d5 = extractvalue { <2 x i8>, <2 x i8>, <2 x i8>, <2 x i8>, <2 x i8>, <2 x i8>, <2 x i8>, <2 x i8> } %d, 5
  %d6 = extractvalue { <2 x i8>, <2 x i8>, <2 x i8>, <2 x i8>, <2 x i8>, <2 x i8>, <2 x i8>, <2 x i8> } %d, 6
  %d7 = extractvalue { <2 x i8>, <2 x i8>, <2 x i8>, <2 x i8>, <2 x i8>, <2 x i8>, <2 x i8>, <2 x i8> } %d, 7
  %r = call <16 x i8> @llvm.vector.interleave8.v16i8(<2 x i8> %d0, <2 x i8> %d1, <2 x i8> %d2, <2 x i8> %d3, <2 x i8> %d4, <2 x i8> %d5, <2 x i8> %d6, <2 x i8> %d7)
  ret <16 x i8> %r
}

define <16 x i8> @tgt(<16 x i8> noundef %v) {
  ret <16 x i8> %v
}
