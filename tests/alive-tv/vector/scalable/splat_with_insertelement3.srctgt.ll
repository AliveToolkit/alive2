define <vscale x 2 x i32> @src(<vscale x 2 x i32> %vec, i32 %val) {
  %vec1 = insertelement <vscale x 2 x i32> %vec, i32 0, i32 0
  %vec2 = insertelement <vscale x 2 x i32> %vec1, i32 0, i32 1
  %vec3 = insertelement <vscale x 2 x i32> %vec2, i32 0, i32 2
  %result = insertelement <vscale x 2 x i32> %vec3, i32 0, i32 3
  ret <vscale x 2 x i32> %result
}

define <vscale x 2 x i32> @tgt(<vscale x 2 x i32> %vec, i32 %val) {
  ret <vscale x 2 x i32> zeroinitializer
}

; TEST-ARGS: --vscale=3
; ERROR: Value mismatch
