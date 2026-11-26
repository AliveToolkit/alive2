; TEST-ARGS: --single-vscale=4
; with vscale=4, %a and %b have 8 lanes and the result has 16;
; lane 2*7+1 of the result is lane 7 of %b

define i32 @src(<vscale x 2 x i32> %a, <vscale x 2 x i32> %b) {
  %r = call <vscale x 4 x i32> @llvm.vector.interleave2.nxv4i32(<vscale x 2 x i32> %a, <vscale x 2 x i32> %b)
  %e = extractelement <vscale x 4 x i32> %r, i64 15
  ret i32 %e
}

define i32 @tgt(<vscale x 2 x i32> %a, <vscale x 2 x i32> %b) {
  %e = extractelement <vscale x 2 x i32> %b, i64 7
  ret i32 %e
}
