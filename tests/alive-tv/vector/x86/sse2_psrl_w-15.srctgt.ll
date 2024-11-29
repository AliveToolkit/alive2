; TEST-ARGS: -disable-undef-input
; disabled undef inputs to accelerate the verification

define <8 x i16> @src(<8 x i16> %v) {
  %1 = call <8 x i16> @llvm.x86.sse2.psrl.w(<8 x i16> %v, <8 x i16> <i16 15, i16 0, i16 0, i16 0, i16 9999, i16 9999, i16 9999, i16 9999>)
  ret <8 x i16> %1
}

define <8 x i16> @tgt(<8 x i16> %v) {
  %tmp = lshr <8 x i16> %v, <i16 15, i16 15, i16 15, i16 15, i16 15, i16 15, i16 15, i16 15>
  ret <8 x i16> %tmp
}

declare <8 x i16> @llvm.x86.sse2.psrl.w(<8 x i16>, <8 x i16>)
