define <vscale x 2 x i32> @src(<vscale x 2 x i32> %vec) {
  %insert = insertelement <vscale x 2 x i32> %vec, i32 0, i32 0
  %shuf = shufflevector <vscale x 2 x i32> %insert, <vscale x 2 x i32> poison, <vscale x 2 x i32> zeroinitializer
  ret <vscale x 2 x i32> %shuf
}

define <vscale x 2 x i32> @tgt(<vscale x 2 x i32> %vec) {
  ret <vscale x 2 x i32> zeroinitializer
}

; TEST-ARGS: --vscale=2
