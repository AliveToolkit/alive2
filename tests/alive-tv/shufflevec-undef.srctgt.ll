define <2 x i32> @src(i32 %a) {
  %ie = insertelement <2 x i32> poison, i32 %a, i32 0
  %ret = shufflevector <2 x i32> %ie, <2 x i32> poison, <2 x i32> <i32 0, i32 0>
  ret <2 x i32> %ret
}

define <2 x i32> @tgt(i32 %a) {
  %ie = insertelement <2 x i32> poison, i32 %a, i32 0
  %ret = insertelement <2 x i32> %ie, i32 %a, i32 1
  ret <2 x i32> %ret
}
