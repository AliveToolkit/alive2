declare i8 @llvm.umul.fix.i8(i8 %a, i8 %b, i32 %scale)
declare <6 x i8> @llvm.umul.fix.v6i8(<6 x i8> %a, <6 x i8> %b, i32 %scale)

define i8 @f1() {
  %res = call i8 @llvm.umul.fix.i8(i8 16, i8 16, i32 4)
  ret i8 %res
}

define i8 @f2() {
  %res = call i8 @llvm.umul.fix.i8(i8 32, i8 32, i32 4)
  ret i8 %res
}

define i8 @f3() {
  %res = call i8 @llvm.umul.fix.i8(i8 127, i8 127, i32 4)
  ret i8 %res
}

define i8 @f4() {
  %res = call i8 @llvm.umul.fix.i8(i8 8, i8 8, i32 4)
  ret i8 %res
}

define i8 @f5() {
  %res = call i8 @llvm.umul.fix.i8(i8 2, i8 2, i32 4)
  ret i8 %res
}

define i8 @f6() {
  %res = call i8 @llvm.umul.fix.i8(i8 1, i8 1, i32 4)
  ret i8 %res
}

define <6 x i8> @f7() {
  %res = call <6 x i8> @llvm.umul.fix.v6i8(<6 x i8> <i8 16, i8 32, i8 127, i8 8, i8 2, i8 1>, <6 x i8> <i8 16, i8 32, i8 127, i8 8, i8 2, i8 1>, i32 4)
  ret <6 x i8> %res
}
