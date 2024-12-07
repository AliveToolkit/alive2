define <vscale x 2 x ptr> @src() {
  %a = getelementptr i8, ptr undef, <vscale x 2 x i64> zeroinitializer
  ret <vscale x 2 x ptr> %a
}

define <2 x ptr> @tgt() {
  ret <2 x ptr> undef
}

; CHECK: ERROR: program doesn't type check!
