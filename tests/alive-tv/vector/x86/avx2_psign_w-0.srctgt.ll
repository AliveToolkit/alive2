define <8 x i32> @src(<8 x i32> %v) {
  %1 = call <8 x i32> @llvm.x86.avx2.psign.d(<8 x i32> %v, <8 x i32>  zeroinitializer)
  ret <8 x i32> %1
}

define <8 x i32> @tgt(<8 x i32> %v) {
  ret <8 x i32> zeroinitializer
}

declare <8 x i32> @llvm.x86.avx2.psign.d(<8 x i32>, <8 x i32>)
