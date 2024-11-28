define <vscale x 2 x ptr> @src() {
  %a = getelementptr i8, ptr undef, <vscale x 2 x i64> zeroinitializer
  ret <vscale x 2 x ptr> %a
}

define <vscale x 2 x ptr> @tgt() {
  ret <vscale x 2 x ptr> undef
}
