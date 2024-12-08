define <2 x i64> @src(<2 x i64> %v) {
  %1 = call <2 x i64> @llvm.x86.sse2.psrl.q(<2 x i64> %v, <2 x i64>  <i64 5, i64 9999>)
  ret <2 x i64> %1
}

define <2 x i64> @tgt(<2 x i64> %v) {
  %tmp = lshr <2 x i64> %v, <i64 5, i64 5>
  ret <2 x i64> %tmp
}

declare <2 x i64> @llvm.x86.sse2.psrl.q(<2 x i64>, <2 x i64>)
