; SKIP-IDENTITY

define <vscale x 1 x i8> @src(<vscale x 1 x i8> %a) vscale_range(1, 2) {
  %v = insertelement <vscale x 1 x i8> %a, i8 -2, i64 3
  ret <vscale x 1 x i8> %v
}

define <vscale x 1 x i8> @tgt(<vscale x 1 x i8> %a) vscale_range(1, 2) {
  ret <vscale x 1 x i8> poison
}

; ERROR: program doesn't type check!
