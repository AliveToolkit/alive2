define <vscale x 2 x i64> @src(<vscale x 2 x i64> %z) {
  %shuf = shufflevector <vscale x 2 x i64> insertelement (<vscale x 2 x i64> poison, i64 0, i32 0), <vscale x 2 x i64> poison, <vscale x 2 x i32> zeroinitializer
  %t3 = mul <vscale x 2 x i64> %shuf, %z
  ret <vscale x 2 x i64> %t3
}

define <vscale x 2 x i64> @tgt(<vscale x 2 x i64> %z) {
  ret <vscale x 2 x i64> zeroinitializer
}
