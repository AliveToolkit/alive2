define <8 x i16> @src() {
  %calltmp = call <8 x i16> @llvm.x86.ssse3.pmadd.ub.sw.128(<16 x i8> <i8 25, i8 87, i8 -119, i8 -75, i8 8, i8 -7, i8 -57, i8 70, i8 -5, i8 19, i8 89, i8 29, i8 11, i8 19, i8 -48, i8 -111>, <16 x i8> <i8 25, i8 32, i8 12, i8 30, i8 14, i8 16, i8 12, i8 2, i8 8, i8 22, i8 28, i8 12, i8 22, i8 8, i8 6, i8 5>)
  ret <8 x i16> %calltmp
}

define <8 x i16> @tgt() {
  ret <8 x i16> <i16 3409, i16 7074, i16 4096, i16 2528, i16 2426, i16 2840, i16 394, i16 1973>
}

declare <8 x i16> @llvm.x86.ssse3.pmadd.ub.sw.128(<16 x i8>, <16 x i8>)
