; SKIP-IDENTITY

define <vscale x 1 x i8> @src(<vscale x 1 x i8> %a) vscale_range(2, 4) {
  %v = insertelement <vscale x 1 x i8> %a, i8 -1, i64 2
  ret <vscale x 1 x i8> %v
}

define <vscale x 1 x i8> @tgt(<vscale x 1 x i8> %a) vscale_range(2, 4) {
  ret <vscale x 1 x i8> poison
}

; ERROR: Unsupported type: <vscale x 1 x i8>
