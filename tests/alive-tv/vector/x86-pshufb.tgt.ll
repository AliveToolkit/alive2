; ModuleID = 'x86-pshufb.ll'
target triple = "x86_64-unknown-unknown"

define <16 x i8> @identity_test(<16 x i8> %InVec) {
  ret <16 x i8> %InVec
}

define <32 x i8> @identity_test_avx2(<32 x i8> %InVec) {
  ret <32 x i8> %InVec
}

define <64 x i8> @identity_test_avx512(<64 x i8> %InVec) {
  ret <64 x i8> %InVec
}

define <16 x i8> @fold_to_zero_vector(<16 x i8> %InVec) {
  ret <16 x i8> zeroinitializer
}

define <32 x i8> @fold_to_zero_vector_avx2(<32 x i8> %InVec) {
  ret <32 x i8> zeroinitializer
}

define <64 x i8> @fold_to_zero_vector_avx512(<64 x i8> %InVec) {
  ret <64 x i8> zeroinitializer
}

define <16 x i8> @splat_test(<16 x i8> %InVec) {
  %1 = shufflevector <16 x i8> %InVec, <16 x i8> undef, <16 x i32> zeroinitializer
  ret <16 x i8> %1
}

define <32 x i8> @splat_test_avx2(<32 x i8> %InVec) {
  %1 = shufflevector <32 x i8> %InVec, <32 x i8> poison, <32 x i32> <i32 0, i32 0, i32 0, i32 0, i32 0, i32 0, i32 0, i32 0, i32 0, i32 0, i32 0, i32 0, i32 0, i32 0, i32 0, i32 0, i32 16, i32 16, i32 16, i32 16, i32 16, i32 16, i32 16, i32 16, i32 16, i32 16, i32 16, i32 16, i32 16, i32 16, i32 16, i32 16>
  ret <32 x i8> %1
}

define <64 x i8> @splat_test_avx512(<64 x i8> %InVec) {
  %1 = shufflevector <64 x i8> %InVec, <64 x i8> poison, <64 x i32> <i32 0, i32 0, i32 0, i32 0, i32 0, i32 0, i32 0, i32 0, i32 0, i32 0, i32 0, i32 0, i32 0, i32 0, i32 0, i32 0, i32 16, i32 16, i32 16, i32 16, i32 16, i32 16, i32 16, i32 16, i32 16, i32 16, i32 16, i32 16, i32 16, i32 16, i32 16, i32 16, i32 32, i32 32, i32 32, i32 32, i32 32, i32 32, i32 32, i32 32, i32 32, i32 32, i32 32, i32 32, i32 32, i32 32, i32 32, i32 32, i32 48, i32 48, i32 48, i32 48, i32 48, i32 48, i32 48, i32 48, i32 48, i32 48, i32 48, i32 48, i32 48, i32 48, i32 48, i32 48>
  ret <64 x i8> %1
}

define <16 x i8> @blend1(<16 x i8> %InVec) {
  %1 = shufflevector <16 x i8> %InVec, <16 x i8> <i8 0, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison>, <16 x i32> <i32 16, i32 1, i32 16, i32 3, i32 16, i32 5, i32 16, i32 7, i32 16, i32 9, i32 16, i32 11, i32 16, i32 13, i32 16, i32 15>
  ret <16 x i8> %1
}

define <16 x i8> @blend2(<16 x i8> %InVec) {
  %1 = shufflevector <16 x i8> %InVec, <16 x i8> <i8 0, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison>, <16 x i32> <i32 16, i32 16, i32 2, i32 3, i32 16, i32 16, i32 6, i32 7, i32 16, i32 16, i32 10, i32 11, i32 16, i32 16, i32 14, i32 15>
  ret <16 x i8> %1
}

define <16 x i8> @blend3(<16 x i8> %InVec) {
  %1 = shufflevector <16 x i8> %InVec, <16 x i8> <i8 0, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison>, <16 x i32> <i32 16, i32 16, i32 16, i32 16, i32 4, i32 5, i32 6, i32 7, i32 16, i32 16, i32 16, i32 16, i32 12, i32 13, i32 14, i32 15>
  ret <16 x i8> %1
}

define <16 x i8> @blend4(<16 x i8> %InVec) {
  %1 = shufflevector <16 x i8> %InVec, <16 x i8> <i8 0, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison>, <16 x i32> <i32 16, i32 16, i32 16, i32 16, i32 16, i32 16, i32 16, i32 16, i32 8, i32 9, i32 10, i32 11, i32 12, i32 13, i32 14, i32 15>
  ret <16 x i8> %1
}

define <16 x i8> @blend5(<16 x i8> %InVec) {
  %1 = shufflevector <16 x i8> %InVec, <16 x i8> <i8 0, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison>, <16 x i32> <i32 0, i32 1, i32 2, i32 3, i32 16, i32 16, i32 16, i32 16, i32 16, i32 16, i32 16, i32 16, i32 16, i32 16, i32 16, i32 16>
  ret <16 x i8> %1
}

define <16 x i8> @blend6(<16 x i8> %InVec) {
  %1 = shufflevector <16 x i8> %InVec, <16 x i8> <i8 0, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison>, <16 x i32> <i32 0, i32 1, i32 16, i32 16, i32 16, i32 16, i32 16, i32 16, i32 16, i32 16, i32 16, i32 16, i32 16, i32 16, i32 16, i32 16>
  ret <16 x i8> %1
}

define <32 x i8> @blend1_avx2(<32 x i8> %InVec) {
  %1 = shufflevector <32 x i8> %InVec, <32 x i8> <i8 0, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 0, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison>, <32 x i32> <i32 32, i32 1, i32 32, i32 3, i32 32, i32 5, i32 32, i32 7, i32 32, i32 9, i32 32, i32 11, i32 32, i32 13, i32 32, i32 15, i32 48, i32 17, i32 48, i32 19, i32 48, i32 21, i32 48, i32 23, i32 48, i32 25, i32 48, i32 27, i32 48, i32 29, i32 48, i32 31>
  ret <32 x i8> %1
}

define <32 x i8> @blend2_avx2(<32 x i8> %InVec) {
  %1 = shufflevector <32 x i8> %InVec, <32 x i8> <i8 0, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 0, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison>, <32 x i32> <i32 32, i32 32, i32 2, i32 3, i32 32, i32 32, i32 6, i32 7, i32 32, i32 32, i32 10, i32 11, i32 32, i32 32, i32 14, i32 15, i32 48, i32 48, i32 18, i32 19, i32 48, i32 48, i32 22, i32 23, i32 48, i32 48, i32 26, i32 27, i32 48, i32 48, i32 30, i32 31>
  ret <32 x i8> %1
}

define <32 x i8> @blend3_avx2(<32 x i8> %InVec) {
  %1 = shufflevector <32 x i8> %InVec, <32 x i8> <i8 0, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 0, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison>, <32 x i32> <i32 32, i32 32, i32 32, i32 32, i32 4, i32 5, i32 6, i32 7, i32 32, i32 32, i32 32, i32 32, i32 12, i32 13, i32 14, i32 15, i32 48, i32 48, i32 48, i32 48, i32 20, i32 21, i32 22, i32 23, i32 48, i32 48, i32 48, i32 48, i32 28, i32 29, i32 30, i32 31>
  ret <32 x i8> %1
}

define <32 x i8> @blend4_avx2(<32 x i8> %InVec) {
  %1 = shufflevector <32 x i8> %InVec, <32 x i8> <i8 0, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 0, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison>, <32 x i32> <i32 32, i32 32, i32 32, i32 32, i32 32, i32 32, i32 32, i32 32, i32 8, i32 9, i32 10, i32 11, i32 12, i32 13, i32 14, i32 15, i32 48, i32 48, i32 48, i32 48, i32 48, i32 48, i32 48, i32 48, i32 24, i32 25, i32 26, i32 27, i32 28, i32 29, i32 30, i32 31>
  ret <32 x i8> %1
}

define <32 x i8> @blend5_avx2(<32 x i8> %InVec) {
  %1 = shufflevector <32 x i8> %InVec, <32 x i8> <i8 0, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 0, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison>, <32 x i32> <i32 0, i32 1, i32 2, i32 3, i32 32, i32 32, i32 32, i32 32, i32 32, i32 32, i32 32, i32 32, i32 32, i32 32, i32 32, i32 32, i32 16, i32 17, i32 18, i32 19, i32 48, i32 48, i32 48, i32 48, i32 48, i32 48, i32 48, i32 48, i32 48, i32 48, i32 48, i32 48>
  ret <32 x i8> %1
}

define <32 x i8> @blend6_avx2(<32 x i8> %InVec) {
  %1 = shufflevector <32 x i8> %InVec, <32 x i8> <i8 0, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 0, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison>, <32 x i32> <i32 0, i32 1, i32 32, i32 32, i32 32, i32 32, i32 32, i32 32, i32 32, i32 32, i32 32, i32 32, i32 32, i32 32, i32 32, i32 32, i32 16, i32 17, i32 48, i32 48, i32 48, i32 48, i32 48, i32 48, i32 48, i32 48, i32 48, i32 48, i32 48, i32 48, i32 48, i32 48>
  ret <32 x i8> %1
}

define <64 x i8> @blend1_avx512(<64 x i8> %InVec) {
  %1 = shufflevector <64 x i8> %InVec, <64 x i8> <i8 0, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 0, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 0, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 0, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison>, <64 x i32> <i32 64, i32 1, i32 64, i32 3, i32 64, i32 5, i32 64, i32 7, i32 64, i32 9, i32 64, i32 11, i32 64, i32 13, i32 64, i32 15, i32 80, i32 17, i32 80, i32 19, i32 80, i32 21, i32 80, i32 23, i32 80, i32 25, i32 80, i32 27, i32 80, i32 29, i32 80, i32 31, i32 96, i32 33, i32 96, i32 35, i32 96, i32 37, i32 96, i32 39, i32 96, i32 41, i32 96, i32 43, i32 96, i32 45, i32 96, i32 47, i32 112, i32 49, i32 112, i32 51, i32 112, i32 53, i32 112, i32 55, i32 112, i32 57, i32 112, i32 59, i32 112, i32 61, i32 112, i32 63>
  ret <64 x i8> %1
}

define <64 x i8> @blend2_avx512(<64 x i8> %InVec) {
  %1 = shufflevector <64 x i8> %InVec, <64 x i8> <i8 0, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 0, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 0, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 0, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison>, <64 x i32> <i32 64, i32 64, i32 2, i32 3, i32 64, i32 64, i32 6, i32 7, i32 64, i32 64, i32 10, i32 11, i32 64, i32 64, i32 14, i32 15, i32 80, i32 80, i32 18, i32 19, i32 80, i32 80, i32 22, i32 23, i32 80, i32 80, i32 26, i32 27, i32 80, i32 80, i32 30, i32 31, i32 96, i32 96, i32 34, i32 35, i32 96, i32 96, i32 38, i32 39, i32 96, i32 96, i32 42, i32 43, i32 96, i32 96, i32 46, i32 47, i32 112, i32 112, i32 50, i32 51, i32 112, i32 112, i32 54, i32 55, i32 112, i32 112, i32 58, i32 59, i32 112, i32 112, i32 62, i32 63>
  ret <64 x i8> %1
}

define <64 x i8> @blend3_avx512(<64 x i8> %InVec) {
  %1 = shufflevector <64 x i8> %InVec, <64 x i8> <i8 0, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 0, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 0, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 0, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison>, <64 x i32> <i32 64, i32 64, i32 64, i32 64, i32 4, i32 5, i32 6, i32 7, i32 64, i32 64, i32 64, i32 64, i32 12, i32 13, i32 14, i32 15, i32 80, i32 80, i32 80, i32 80, i32 20, i32 21, i32 22, i32 23, i32 80, i32 80, i32 80, i32 80, i32 28, i32 29, i32 30, i32 31, i32 96, i32 96, i32 96, i32 96, i32 36, i32 37, i32 38, i32 39, i32 96, i32 96, i32 96, i32 96, i32 44, i32 45, i32 46, i32 47, i32 112, i32 112, i32 112, i32 112, i32 52, i32 53, i32 54, i32 55, i32 112, i32 112, i32 112, i32 112, i32 60, i32 61, i32 62, i32 63>
  ret <64 x i8> %1
}

define <64 x i8> @blend4_avx512(<64 x i8> %InVec) {
  %1 = shufflevector <64 x i8> %InVec, <64 x i8> <i8 0, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 0, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 0, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 0, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison>, <64 x i32> <i32 64, i32 64, i32 64, i32 64, i32 64, i32 64, i32 64, i32 64, i32 8, i32 9, i32 10, i32 11, i32 12, i32 13, i32 14, i32 15, i32 80, i32 80, i32 80, i32 80, i32 80, i32 80, i32 80, i32 80, i32 24, i32 25, i32 26, i32 27, i32 28, i32 29, i32 30, i32 31, i32 96, i32 96, i32 96, i32 96, i32 96, i32 96, i32 96, i32 96, i32 40, i32 41, i32 42, i32 43, i32 44, i32 45, i32 46, i32 47, i32 112, i32 112, i32 112, i32 112, i32 112, i32 112, i32 112, i32 112, i32 56, i32 57, i32 58, i32 59, i32 60, i32 61, i32 62, i32 63>
  ret <64 x i8> %1
}

define <64 x i8> @blend5_avx512(<64 x i8> %InVec) {
  %1 = shufflevector <64 x i8> %InVec, <64 x i8> <i8 0, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 0, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 0, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 0, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison>, <64 x i32> <i32 0, i32 1, i32 2, i32 3, i32 64, i32 64, i32 64, i32 64, i32 64, i32 64, i32 64, i32 64, i32 64, i32 64, i32 64, i32 64, i32 16, i32 17, i32 18, i32 19, i32 80, i32 80, i32 80, i32 80, i32 80, i32 80, i32 80, i32 80, i32 80, i32 80, i32 80, i32 80, i32 32, i32 33, i32 34, i32 35, i32 96, i32 96, i32 96, i32 96, i32 96, i32 96, i32 96, i32 96, i32 96, i32 96, i32 96, i32 96, i32 48, i32 49, i32 50, i32 51, i32 112, i32 112, i32 112, i32 112, i32 112, i32 112, i32 112, i32 112, i32 112, i32 112, i32 112, i32 112>
  ret <64 x i8> %1
}

define <64 x i8> @blend6_avx512(<64 x i8> %InVec) {
  %1 = shufflevector <64 x i8> %InVec, <64 x i8> <i8 0, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 0, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 0, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 0, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison>, <64 x i32> <i32 0, i32 1, i32 64, i32 64, i32 64, i32 64, i32 64, i32 64, i32 64, i32 64, i32 64, i32 64, i32 64, i32 64, i32 64, i32 64, i32 16, i32 17, i32 80, i32 80, i32 80, i32 80, i32 80, i32 80, i32 80, i32 80, i32 80, i32 80, i32 80, i32 80, i32 80, i32 80, i32 32, i32 33, i32 96, i32 96, i32 96, i32 96, i32 96, i32 96, i32 96, i32 96, i32 96, i32 96, i32 96, i32 96, i32 96, i32 96, i32 48, i32 49, i32 112, i32 112, i32 112, i32 112, i32 112, i32 112, i32 112, i32 112, i32 112, i32 112, i32 112, i32 112, i32 112, i32 112>
  ret <64 x i8> %1
}

define <16 x i8> @movq_idiom(<16 x i8> %InVec) {
  %1 = shufflevector <16 x i8> %InVec, <16 x i8> <i8 0, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison>, <16 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7, i32 16, i32 16, i32 16, i32 16, i32 16, i32 16, i32 16, i32 16>
  ret <16 x i8> %1
}

define <32 x i8> @movq_idiom_avx2(<32 x i8> %InVec) {
  %1 = shufflevector <32 x i8> %InVec, <32 x i8> <i8 0, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 0, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison>, <32 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7, i32 32, i32 32, i32 32, i32 32, i32 32, i32 32, i32 32, i32 32, i32 48, i32 48, i32 48, i32 48, i32 48, i32 48, i32 48, i32 48, i32 48, i32 48, i32 48, i32 48, i32 48, i32 48, i32 48, i32 48>
  ret <32 x i8> %1
}

define <64 x i8> @movq_idiom_avx512(<64 x i8> %InVec) {
  %1 = shufflevector <64 x i8> %InVec, <64 x i8> <i8 0, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 0, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 0, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 0, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison>, <64 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7, i32 64, i32 64, i32 64, i32 64, i32 64, i32 64, i32 64, i32 64, i32 80, i32 80, i32 80, i32 80, i32 80, i32 80, i32 80, i32 80, i32 80, i32 80, i32 80, i32 80, i32 80, i32 80, i32 80, i32 80, i32 96, i32 96, i32 96, i32 96, i32 96, i32 96, i32 96, i32 96, i32 96, i32 96, i32 96, i32 96, i32 96, i32 96, i32 96, i32 96, i32 112, i32 112, i32 112, i32 112, i32 112, i32 112, i32 112, i32 112, i32 112, i32 112, i32 112, i32 112, i32 112, i32 112, i32 112, i32 112>
  ret <64 x i8> %1
}

define <16 x i8> @permute1(<16 x i8> %InVec) {
  %1 = shufflevector <16 x i8> %InVec, <16 x i8> poison, <16 x i32> <i32 4, i32 5, i32 6, i32 7, i32 4, i32 5, i32 6, i32 7, i32 12, i32 13, i32 14, i32 15, i32 12, i32 13, i32 14, i32 15>
  ret <16 x i8> %1
}

define <16 x i8> @permute2(<16 x i8> %InVec) {
  %1 = shufflevector <16 x i8> %InVec, <16 x i8> poison, <16 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7, i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7>
  ret <16 x i8> %1
}

define <32 x i8> @permute1_avx2(<32 x i8> %InVec) {
  %1 = shufflevector <32 x i8> %InVec, <32 x i8> poison, <32 x i32> <i32 4, i32 5, i32 6, i32 7, i32 4, i32 5, i32 6, i32 7, i32 12, i32 13, i32 14, i32 15, i32 12, i32 13, i32 14, i32 15, i32 20, i32 21, i32 22, i32 23, i32 20, i32 21, i32 22, i32 23, i32 28, i32 29, i32 30, i32 31, i32 28, i32 29, i32 30, i32 31>
  ret <32 x i8> %1
}

define <32 x i8> @permute2_avx2(<32 x i8> %InVec) {
  %1 = shufflevector <32 x i8> %InVec, <32 x i8> poison, <32 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7, i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7, i32 16, i32 17, i32 18, i32 19, i32 20, i32 21, i32 22, i32 23, i32 16, i32 17, i32 18, i32 19, i32 20, i32 21, i32 22, i32 23>
  ret <32 x i8> %1
}

define <64 x i8> @permute1_avx512(<64 x i8> %InVec) {
  %1 = shufflevector <64 x i8> %InVec, <64 x i8> poison, <64 x i32> <i32 4, i32 5, i32 6, i32 7, i32 4, i32 5, i32 6, i32 7, i32 12, i32 13, i32 14, i32 15, i32 12, i32 13, i32 14, i32 15, i32 20, i32 21, i32 22, i32 23, i32 20, i32 21, i32 22, i32 23, i32 28, i32 29, i32 30, i32 31, i32 28, i32 29, i32 30, i32 31, i32 36, i32 37, i32 38, i32 39, i32 36, i32 37, i32 38, i32 39, i32 44, i32 45, i32 46, i32 47, i32 44, i32 45, i32 46, i32 47, i32 52, i32 53, i32 54, i32 55, i32 52, i32 53, i32 54, i32 55, i32 60, i32 61, i32 62, i32 63, i32 60, i32 61, i32 62, i32 63>
  ret <64 x i8> %1
}

define <64 x i8> @permute2_avx512(<64 x i8> %InVec) {
  %1 = shufflevector <64 x i8> %InVec, <64 x i8> poison, <64 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7, i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7, i32 16, i32 17, i32 18, i32 19, i32 20, i32 21, i32 22, i32 23, i32 16, i32 17, i32 18, i32 19, i32 20, i32 21, i32 22, i32 23, i32 32, i32 33, i32 34, i32 35, i32 36, i32 37, i32 38, i32 39, i32 32, i32 33, i32 34, i32 35, i32 36, i32 37, i32 38, i32 39, i32 48, i32 49, i32 50, i32 51, i32 52, i32 53, i32 54, i32 55, i32 48, i32 49, i32 50, i32 51, i32 52, i32 53, i32 54, i32 55>
  ret <64 x i8> %1
}

define <16 x i8> @identity_test2_2(<16 x i8> %InVec) {
  ret <16 x i8> %InVec
}

define <32 x i8> @identity_test_avx2_2(<32 x i8> %InVec) {
  ret <32 x i8> %InVec
}

define <64 x i8> @identity_test_avx512_2(<64 x i8> %InVec) {
  ret <64 x i8> %InVec
}

define <16 x i8> @fold_to_zero_vector_2(<16 x i8> %InVec) {
  ret <16 x i8> zeroinitializer
}

define <32 x i8> @fold_to_zero_vector_avx2_2(<32 x i8> %InVec) {
  ret <32 x i8> zeroinitializer
}

define <64 x i8> @fold_to_zero_vector_avx512_2(<64 x i8> %InVec) {
  ret <64 x i8> zeroinitializer
}

define <16 x i8> @permute3(<16 x i8> %InVec) {
  %1 = shufflevector <16 x i8> %InVec, <16 x i8> poison, <16 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7, i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7>
  ret <16 x i8> %1
}

define <32 x i8> @permute3_avx2(<32 x i8> %InVec) {
  %1 = shufflevector <32 x i8> %InVec, <32 x i8> poison, <32 x i32> <i32 4, i32 5, i32 6, i32 7, i32 4, i32 5, i32 6, i32 7, i32 12, i32 13, i32 14, i32 15, i32 12, i32 13, i32 14, i32 15, i32 20, i32 21, i32 22, i32 23, i32 20, i32 21, i32 22, i32 23, i32 28, i32 29, i32 30, i32 31, i32 28, i32 29, i32 30, i32 31>
  ret <32 x i8> %1
}

define <64 x i8> @permute3_avx512(<64 x i8> %InVec) {
  %1 = shufflevector <64 x i8> %InVec, <64 x i8> poison, <64 x i32> <i32 4, i32 5, i32 6, i32 7, i32 4, i32 5, i32 6, i32 7, i32 12, i32 13, i32 14, i32 15, i32 12, i32 13, i32 14, i32 15, i32 20, i32 21, i32 22, i32 23, i32 20, i32 21, i32 22, i32 23, i32 28, i32 29, i32 30, i32 31, i32 28, i32 29, i32 30, i32 31, i32 36, i32 37, i32 38, i32 39, i32 36, i32 37, i32 38, i32 39, i32 44, i32 45, i32 46, i32 47, i32 44, i32 45, i32 46, i32 47, i32 52, i32 53, i32 54, i32 55, i32 52, i32 53, i32 54, i32 55, i32 60, i32 61, i32 62, i32 63, i32 60, i32 61, i32 62, i32 63>
  ret <64 x i8> %1
}

define <16 x i8> @fold_with_undef_elts(<16 x i8> %InVec) {
  %1 = shufflevector <16 x i8> %InVec, <16 x i8> <i8 0, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison>, <16 x i32> <i32 0, i32 16, i32 undef, i32 16, i32 1, i32 16, i32 undef, i32 16, i32 2, i32 16, i32 undef, i32 16, i32 3, i32 16, i32 undef, i32 16>
  ret <16 x i8> %1
}

define <32 x i8> @fold_with_undef_elts_avx2(<32 x i8> %InVec) {
  %1 = shufflevector <32 x i8> %InVec, <32 x i8> <i8 0, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 0, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison, i8 poison>, <32 x i32> <i32 0, i32 32, i32 undef, i32 32, i32 1, i32 32, i32 undef, i32 32, i32 2, i32 32, i32 undef, i32 32, i32 3, i32 32, i32 undef, i32 32, i32 16, i32 48, i32 undef, i32 48, i32 17, i32 48, i32 undef, i32 48, i32 18, i32 48, i32 undef, i32 48, i32 19, i32 48, i32 undef, i32 48>
  ret <32 x i8> %1
}

define <16 x i8> @fold_with_allundef_elts(<16 x i8> %InVec) {
  ret <16 x i8> undef
}

define <32 x i8> @fold_with_allundef_elts_avx2(<32 x i8> %InVec) {
  ret <32 x i8> undef
}

define <16 x i8> @demanded_elts_insertion(<16 x i8> %InVec, <16 x i8> %BaseMask, i8 %M0, i8 %M15) {
  %1 = tail call <16 x i8> @llvm.x86.ssse3.pshuf.b.128(<16 x i8> %InVec, <16 x i8> %BaseMask)
  %2 = shufflevector <16 x i8> %1, <16 x i8> undef, <16 x i32> <i32 undef, i32 14, i32 13, i32 12, i32 11, i32 10, i32 9, i32 8, i32 7, i32 6, i32 5, i32 4, i32 3, i32 2, i32 1, i32 undef>
  ret <16 x i8> %2
}

define <32 x i8> @demanded_elts_insertion_avx2(<32 x i8> %InVec, <32 x i8> %BaseMask, i8 %M0, i8 %M22) {
  %1 = tail call <32 x i8> @llvm.x86.avx2.pshuf.b(<32 x i8> %InVec, <32 x i8> %BaseMask)
  %2 = shufflevector <32 x i8> %1, <32 x i8> undef, <32 x i32> <i32 undef, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7, i32 8, i32 9, i32 10, i32 11, i32 12, i32 13, i32 14, i32 15, i32 16, i32 17, i32 18, i32 19, i32 20, i32 21, i32 undef, i32 23, i32 24, i32 25, i32 26, i32 27, i32 28, i32 29, i32 30, i32 31>
  ret <32 x i8> %2
}

; Function Attrs: nounwind readnone
declare <16 x i8> @llvm.x86.ssse3.pshuf.b.128(<16 x i8>, <16 x i8>) #0

; Function Attrs: nounwind readnone
declare <32 x i8> @llvm.x86.avx2.pshuf.b(<32 x i8>, <32 x i8>) #0

; Function Attrs: nounwind readnone
declare <64 x i8> @llvm.x86.avx512.pshuf.b.512(<64 x i8>, <64 x i8>) #0

attributes #0 = { nounwind readnone }
