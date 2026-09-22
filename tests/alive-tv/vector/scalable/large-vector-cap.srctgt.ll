; TEST-ARGS: --single-vscale=2048
; SKIP-IDENTITY
; ERROR: Vector type is too large

define <vscale x 64 x i1> @src(<vscale x 64 x i1> %v) {
  ret <vscale x 64 x i1> %v
}

define <vscale x 64 x i1> @tgt(<vscale x 64 x i1> %v) {
  ret <vscale x 64 x i1> %v
}
