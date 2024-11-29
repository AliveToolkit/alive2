; TEST-ARGS: -disable-undef-input
; disabled undef inputs to accelerate the verification

define <16 x i16> @src(<16 x i16> %v) {
  %1 = call <16 x i16> @llvm.x86.avx2.psrl.w(<16 x i16> %v, <8 x i16> <i16 15, i16 0, i16 0, i16 0, i16 9999, i16 9999, i16 9999, i16 9999>)
  ret <16 x i16> %1
}

define <16 x i16> @tgt(<16 x i16> %v) {
  %tmp = lshr <16 x i16> %v, <i16 15, i16 15, i16 15, i16 15, i16 15, i16 15, i16 15, i16 15, i16 15, i16 15, i16 15, i16 15, i16 15, i16 15, i16 15, i16 15>
  ret <16 x i16> %tmp
}

declare <16 x i16> @llvm.x86.avx2.psrl.w(<16 x i16>, <8 x i16>)
