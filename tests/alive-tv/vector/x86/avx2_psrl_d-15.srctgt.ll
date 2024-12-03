define <8 x i32> @src(<8 x i32> %v) {
  %1 = call <8 x i32> @llvm.x86.avx2.psrl.d(<8 x i32> %v, <4 x i32> <i32 15, i32 0, i32 9999, i32 9999>)
  ret <8 x i32> %1
}

define <8 x i32> @tgt(<8 x i32> %v) {
  %tmp = lshr <8 x i32> %v, <i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15, i32 15>
  ret <8 x i32> %tmp
}

declare <8 x i32> @llvm.x86.avx2.psrl.d(<8 x i32>, <4 x i32>)
