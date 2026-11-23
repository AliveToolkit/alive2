; odd factor; with the default vscale=2 each input has 4 lanes,
; and lane 3*1+2 of the result is lane 1 of %c

define i8 @src(<vscale x 2 x i8> %a, <vscale x 2 x i8> %b, <vscale x 2 x i8> %c) {
  %r = call <vscale x 6 x i8> @llvm.vector.interleave3.nxv6i8(<vscale x 2 x i8> %a, <vscale x 2 x i8> %b, <vscale x 2 x i8> %c)
  %e = extractelement <vscale x 6 x i8> %r, i64 5
  ret i8 %e
}

define i8 @tgt(<vscale x 2 x i8> %a, <vscale x 2 x i8> %b, <vscale x 2 x i8> %c) {
  %e = extractelement <vscale x 2 x i8> %c, i64 1
  ret i8 %e
}
