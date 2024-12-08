define <4 x i64> @src(<4 x i64> %v) {
  %1 = call <4 x i64> @llvm.x86.avx2.psrl.q(<4 x i64> %v, <2 x i64>  <i64 0, i64 poison>)
  ret <4 x i64> %1
}

define <4 x i64> @tgt(<4 x i64> %v) {
  ret <4 x i64> %v
}

declare <4 x i64> @llvm.x86.avx2.psrl.q(<4 x i64>, <2 x i64>)
