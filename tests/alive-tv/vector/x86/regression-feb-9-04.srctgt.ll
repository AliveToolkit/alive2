define <8 x i32> @src() {
  %calltmp = call <8 x i32> @llvm.x86.avx2.phadd.d(<8 x i32> <i32 1722070598, i32 302159761, i32 -203316396, i32 -2123912179, i32 1892339105, i32 -1274131692, i32 744247972, i32 2066006689>, <8 x i32> <i32 21, i32 17, i32 38, i32 31, i32 39, i32 16, i32 32, i32 16>)
  ret <8 x i32> %calltmp
}

define <8 x i32> @tgt() {
  ret <8 x i32> <i32 2024230359, i32 1967738721, i32 38, i32 69, i32 618207413, i32 -1484712635, i32 55, i32 48>
}

declare <8 x i32> @llvm.x86.avx2.phadd.d(<8 x i32>, <8 x i32>)
