define <2 x half> @src() {
  %t3 = fdiv <2 x half> zeroinitializer, zeroinitializer
  %t4 = fmul <2 x half> zeroinitializer, zeroinitializer
  %t5 = shufflevector <2 x half> %t3, <2 x half> %t4, <2 x i32> <i32 0, i32 2>
  ret <2 x half> %t5
}

define <2 x half> @tgt() {
  %t3 = fdiv <2 x half> zeroinitializer, zeroinitializer
  %t4 = fmul <2 x half> zeroinitializer, <half 0.000000e+00, half undef>
  %t5 = shufflevector <2 x half> %t3, <2 x half> %t4, <2 x i32> <i32 0, i32 2>
  ret <2 x half> %t5
}
