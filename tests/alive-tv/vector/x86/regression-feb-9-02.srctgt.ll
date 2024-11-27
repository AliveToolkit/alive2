define <8 x i32> @src() {
entry:
  %calltmp = call <8 x i32> @llvm.x86.avx2.psra.d(<8 x i32> <i32 -1216751044, i32 1436703847, i32 1582383160, i32 -1379019227, i32 -1448759623, i32 500738486, i32 -615243513, i32 -729809403>, <4 x i32> <i32 2, i32 2, i32 38, i32 11>)
  ret <8 x i32> %calltmp
}
define <8 x i32> @tgt() {
entry:
  ret <8 x i32> <i32 -1, i32 0, i32 0, i32 -1, i32 -1, i32 0, i32 -1, i32 -1>
}

declare <8 x i32> @llvm.x86.avx2.psra.d(<8 x i32>, <4 x i32>)