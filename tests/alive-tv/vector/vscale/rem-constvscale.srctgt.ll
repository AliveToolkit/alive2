; SKIP-IDENTITY

define <vscale x 2 x i8> @src(<vscale x 2 x i8> %x) vscale_range(2, 2) {
  %rem.i = srem <vscale x 2 x i8> %x, splat(i8 2)
  %cmp.i = icmp slt <vscale x 2 x i8> %rem.i, zeroinitializer
  %add.i = select <vscale x 2 x i1> %cmp.i, <vscale x 2 x i8> splat(i8 2), <vscale x 2 x i8> zeroinitializer
  ret <vscale x 2 x i8> %add.i
}

define <vscale x 2 x i8> @tgt(<vscale x 2 x i8> %x) vscale_range(2, 2) {
  %rem.i = srem <vscale x 2 x i8> %x, splat(i8 2)
  %tmp1 = and <vscale x 2 x i8> %rem.i, splat(i8 2)
  ret <vscale x 2 x i8> %tmp1
}

; ERROR: program doesn't type check!
