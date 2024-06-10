define i8 @src1(i32 range(i32 0, 10) %x, i32 range(i32 0, 10) %y) {
  %cmp = call i8 @llvm.scmp.i32.i8(i32 %x, i32 %y)
  ret i8 %cmp
}

define i8 @tgt1(i32 %x, i32 %y) {
  %cmp = call i8 @llvm.ucmp.i32.i8(i32 %x, i32 %y)
  ret i8 %cmp
}

define <2 x i8> @src2(<2 x i32> range(i32 0, 10) %x, <2 x i32> range(i32 0, 10) %y) {
  %cmp = call <2 x i8> @llvm.scmp.v2i32.v2i8(<2 x i32> %x, <2 x i32> %y)
  ret <2 x i8> %cmp
}

define <2 x i8> @tgt2(<2 x i32> %x, <2 x i32> %y) {
  %cmp = call <2 x i8> @llvm.ucmp.v2i32.v2i8(<2 x i32> %x, <2 x i32> %y)
  ret <2 x i8> %cmp
}
