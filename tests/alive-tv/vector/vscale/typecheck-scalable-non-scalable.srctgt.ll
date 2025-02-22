; SKIP-IDENTITY

define <vscale x 1 x i8> @src(<vscale x 1 x i8> %a) vscale_range(1, 2) {
  %v = insertelement <vscale x 1 x i8> %a, i8 -2, i64 3
  ret <vscale x 1 x i8> %v
}

define <1 x i8> @tgt(<1 x i8> %a) vscale_range(1, 2) {
  ret <1 x i8> poison
}

; ERROR: Unsupported type: <vscale x 1 x i8>
