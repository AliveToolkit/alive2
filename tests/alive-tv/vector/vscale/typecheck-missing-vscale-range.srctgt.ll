; SKIP-IDENTITY

define <vscale x 1 x i8> @src(<vscale x 1 x i8> %a) {
  %v = insertelement <vscale x 1 x i8> %a, i8 -2, i64 3
  ret <vscale x 1 x i8> %v
}

define <vscale x 1 x i8> @tgt(<vscale x 1 x i8> %a) {
  ret <vscale x 1 x i8> poison
}

; ERROR: program doesn't type check!
