define <8 x i32> @src(<8 x i32> %v) {
  %1 = call <8 x i32> @llvm.x86.avx2.psign.d(<8 x i32> %v, <8 x i32> <i32 -8, i32 -8, i32 -8, i32 -8, i32 -8, i32 -8, i32 -8, i32 -8>)
  ret <8 x i32> %1
}

define <8 x i32> @tgt(<8 x i32> %v) {
  %neg = sub  <8 x i32> zeroinitializer, %v
  ret <8 x i32> %neg
}

declare <8 x i32> @llvm.x86.avx2.psign.d(<8 x i32>, <8 x i32>)
