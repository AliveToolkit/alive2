define <4 x i32> @src(<4 x i32> %v) {
  %1 = call <4 x i32> @llvm.x86.sse2.psrl.d(<4 x i32> %v, <4 x i32> <i32 3, i32 0, i32 9999, i32 9999>)
  ret <4 x i32> %1
}

define <4 x i32> @tgt(<4 x i32> %v) {
  %tmp = lshr <4 x i32> %v, <i32 5, i32 5, i32 5, i32 5>
  ret <4 x i32> %tmp
}

declare <4 x i32> @llvm.x86.sse2.psrl.d(<4 x i32>, <4 x i32>)

; ERROR: Value mismatch
