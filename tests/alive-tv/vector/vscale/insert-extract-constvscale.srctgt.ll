define i8 @src(<vscale x 1 x i8> %a) vscale_range(4, 4) {
  %v = insertelement <vscale x 1 x i8> %a, i8 -1, i64 2
  %r = extractelement <vscale x 1 x i8> %v, i64 2
  ret i8 %r
}

define i8 @tgt(<vscale x 1 x i8> %a) vscale_range(4, 4) {
  ret i8 -1
}
