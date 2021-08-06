define <16 x i16> @src(<16 x i16> %v) {
  %1 = call <16 x i16> @llvm.x86.avx2.psrl.w(<16 x i16> %v, <8 x i16> <i16 0, i16 0, i16 1, i16 0, i16 0, i16 0, i16 0, i16 0>)
  ret <16 x i16> %1
}

define <16 x i16> @tgt(<16 x i16> %v) {
  ret <16 x i16> %v
}

declare <16 x i16> @llvm.x86.avx2.psrl.w(<16 x i16>, <8 x i16>)

; source = <0, 0, 0 ... >, tgt = <undef, 0, 0, ...> when %input = <undef, 0, 0, ...> 
; ERROR: Target's return value is more undefined

