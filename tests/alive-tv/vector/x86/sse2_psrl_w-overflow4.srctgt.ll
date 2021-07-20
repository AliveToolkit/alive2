define <8 x i16> @src(<8 x i16> %v) {
  %1 = call <8 x i16> @llvm.x86.sse2.psrl.w(<8 x i16> %v, <8 x i16> <i16 0, i16 0, i16 1, i16 0, i16 0, i16 0, i16 0, i16 0>)
  ret <8 x i16> %1
}

define <8 x i16> @tgt(<8 x i16> %v) {
  ret <8 x i16> %v
}

declare <8 x i16> @llvm.x86.sse2.psrl.w(<8 x i16>, <8 x i16>)

; source = <0, 0, 0 ... >, tgt = <undef, 0, 0, ...> when %input = <undef, 0, 0, ...> 
; ERROR: Target's return value is more undefined

