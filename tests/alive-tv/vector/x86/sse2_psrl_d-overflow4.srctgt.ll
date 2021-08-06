define <4 x i32> @src(<4 x i32> %v) {
  %1 = call <4 x i32> @llvm.x86.sse2.psrl.d(<4 x i32> %v, <4 x i32> <i32 0, i32 1, i32 0, i32 0>)
  ret <4 x i32> %1
}

define <4 x i32> @tgt(<4 x i32> %v) {
  ret <4 x i32> %v
}

declare <4 x i32> @llvm.x86.sse2.psrl.d(<4 x i32>, <4 x i32>)

; source = <0, 0, 0 ... >, tgt = <undef, 0, 0, ...> when %input = <undef, 0, 0, ...> 
; ERROR: Target's return value is more undefined
