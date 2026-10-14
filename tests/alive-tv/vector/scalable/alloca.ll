; TEST-ARGS: --single-vscale=2
; CHECK: Transformation seems to be correct!

define <vscale x 4 x i32> @src(<vscale x 4 x i32> %val) {
  %ptr = alloca <vscale x 4 x i32>, align 16
  store <vscale x 4 x i32> %val, ptr %ptr, align 16
  %loaded = load <vscale x 4 x i32>, ptr %ptr, align 16
  ret <vscale x 4 x i32> %loaded
}

define <vscale x 4 x i32> @tgt(<vscale x 4 x i32> %val) {
  ret <vscale x 4 x i32> %val
}
