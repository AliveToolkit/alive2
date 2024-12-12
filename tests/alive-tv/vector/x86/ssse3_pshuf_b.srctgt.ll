define <16 x i8> @src() {
  %call = tail call <16 x i8> @llvm.x86.ssse3.pshuf.b.128(<16 x i8> <i8 0, i8 1, i8 2, i8 3, i8 4, i8 5, i8 6, i8 7, i8 8, i8 9, i8 10, i8 11, i8 12, i8 13, i8 14, i8 15>,  <16 x i8> <i8 16, i8 15, i8 32, i8 33, i8 34, i8 48, i8 64, i8 65, i8 128, i8 129, i8 255, i8 -1, i8 -16, i8 -33, i8 0, i8 poison>)
  ret <16 x i8> %call
}

define <16 x i8> @tgt() {
  ret <16 x i8> <i8 0, i8 15, i8 0, i8 1, i8 2, i8 0, i8 0, i8 1, i8 0, i8 0, i8 0, i8 0, i8 0, i8 0, i8 0, i8 poison>
}
