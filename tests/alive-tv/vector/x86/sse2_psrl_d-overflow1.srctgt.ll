define <4 x i32> @src(<4 x i32> %v) {
  %1 = call <4 x i32> @llvm.x86.sse2.psrl.d(<4 x i32> %v, <4 x i32> <i32 15, i32 15, i32 15, i32 15>)
  ret <4 x i32> %1
}

define <4 x i32> @tgt(<4 x i32> %v) {
  ret <4 x i32> zeroinitializer
}

declare <4 x i32> @llvm.x86.sse2.psrl.d(<4 x i32>, <4 x i32>)
