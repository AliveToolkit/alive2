; test found by Stefan's random tester

define <8 x i16> @src() {
  %calltmp = call <8 x i16> @llvm.x86.sse2.pavg.w(<8 x i16> <i16 5292, i16 65516, i16 4810, i16 37856, i16 55071, i16 1957, i16 49088, i16 24990 >, <8 x i16> <i16 37, i16 21, i16 36, i16 21, i16 10, i16 27, i16 1, i16 9 >)
  ret <8 x i16> %calltmp
}
define <8 x i16> @tgt() {
  ret <8 x i16> <i16 2665, i16 32769, i16 2423, i16 18939, i16 27541, i16 992, i16 24545, i16 12500 >
}
declare <8 x i16> @llvm.x86.sse2.pavg.w(<8 x i16>, <8 x i16>)
