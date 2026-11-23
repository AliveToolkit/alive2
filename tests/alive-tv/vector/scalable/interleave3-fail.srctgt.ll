; ERROR: Value mismatch

define i8 @src(<vscale x 2 x i8> noundef %a, <vscale x 2 x i8> noundef %b, <vscale x 2 x i8> noundef %c) {
  %r = call <vscale x 6 x i8> @llvm.vector.interleave3.nxv6i8(<vscale x 2 x i8> %a, <vscale x 2 x i8> %b, <vscale x 2 x i8> %c)
  %e = extractelement <vscale x 6 x i8> %r, i64 5
  ret i8 %e
}

define i8 @tgt(<vscale x 2 x i8> noundef %a, <vscale x 2 x i8> noundef %b, <vscale x 2 x i8> noundef %c) {
  %e = extractelement <vscale x 2 x i8> %b, i64 1
  ret i8 %e
}
