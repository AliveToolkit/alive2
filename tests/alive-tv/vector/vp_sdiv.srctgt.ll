
declare <2 x i2> @llvm.vp.sdiv.v2i2 (<2 x i2>, <2 x i2>, <2 x i1>, i32)

define <2 x i2> @src(<2 x i2> %x, <2 x i2> %y) {
  %r1 = call <2 x i2> @llvm.vp.sdiv.v2i2(<2 x i2> %x, <2 x i2> %y, <2 x i1> <i1 1, i1 0>, i32 4)
  %r2 = call <2 x i2> @llvm.vp.sdiv.v2i2(<2 x i2> %x, <2 x i2> %y, <2 x i1> <i1 0, i1 1>, i32 4)
  %r = shufflevector <2 x i2> %r1, <2 x i2>%r2, <2 x i32> <i32 0, i32 3>
  ret <2 x i2> %r
}

define <2 x i2> @tgt(<2 x i2> %x, <2 x i2> %y) {
  %r = sdiv <2 x i2> %x, %y
  ret <2 x i2> %r
}
