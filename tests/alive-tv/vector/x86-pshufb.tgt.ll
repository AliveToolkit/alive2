target triple = "x86_64-unknown-unknown"

define <16 x i8> @id(<16 x i8> %x) {
  ret <16 x i8> %x
}

define <32 x i8> @id_avx2(<32 x i8> %x) {
  ret <32 x i8> %x
}

define <64 x i8> @id_avx512(<64 x i8> %x) {
  ret <64 x i8> %x
}

define <16 x i8> @zero(<16 x i8> %x) {
  ret <16 x i8> zeroinitializer
}

define <32 x i8> @zero_avx2(<32 x i8> %x) {
  ret <32 x i8> zeroinitializer
}

define <64 x i8> @zero_avx512(<64 x i8> %x) {
  ret <64 x i8> zeroinitializer
}

define <16 x i8> @splat(<16 x i8> %x) {
  %1 = shufflevector <16 x i8> %x, <16 x i8> undef, <16 x i32> zeroinitializer
  ret <16 x i8> %1
}

define <32 x i8> @splat_avx2(<32 x i8> %x) {
  %1 = shufflevector <32 x i8> %x, <32 x i8> poison, <32 x i32> <i32 0, i32 0, i32 0, i32 0, i32 0, i32 0, i32 0, i32 0, i32 0, i32 0, i32 0, i32 0, i32 0, i32 0, i32 0, i32 0, i32 16, i32 16, i32 16, i32 16, i32 16, i32 16, i32 16, i32 16, i32 16, i32 16, i32 16, i32 16, i32 16, i32 16, i32 16, i32 16>
  ret <32 x i8> %1
}

define <64 x i8> @splat_avx512(<64 x i8> %x) {
  %1 = shufflevector <64 x i8> %x, <64 x i8> poison, <64 x i32> <i32 0, i32 0, i32 0, i32 0, i32 0, i32 0, i32 0, i32 0, i32 0, i32 0, i32 0, i32 0, i32 0, i32 0, i32 0, i32 0, i32 16, i32 16, i32 16, i32 16, i32 16, i32 16, i32 16, i32 16, i32 16, i32 16, i32 16, i32 16, i32 16, i32 16, i32 16, i32 16, i32 32, i32 32, i32 32, i32 32, i32 32, i32 32, i32 32, i32 32, i32 32, i32 32, i32 32, i32 32, i32 32, i32 32, i32 32, i32 32, i32 48, i32 48, i32 48, i32 48, i32 48, i32 48, i32 48, i32 48, i32 48, i32 48, i32 48, i32 48, i32 48, i32 48, i32 48, i32 48>
  ret <64 x i8> %1
}

define <16 x i8> @undef(<16 x i8> %x) {
  ret <16 x i8> undef
}

define <16 x i8> @param(<16 x i8> %x, <16 x i8> %mask) {
  %1 = tail call <16 x i8> @llvm.x86.ssse3.pshuf.b.128(<16 x i8> %x, <16 x i8> %mask)
  ret <16 x i8> %1
}

declare <16 x i8> @llvm.x86.ssse3.pshuf.b.128(<16 x i8>, <16 x i8>)
declare <32 x i8> @llvm.x86.avx2.pshuf.b(<32 x i8>, <32 x i8>)
declare <64 x i8> @llvm.x86.avx512.pshuf.b.512(<64 x i8>, <64 x i8>)
