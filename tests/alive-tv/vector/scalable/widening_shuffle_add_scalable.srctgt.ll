define <vscale x 4 x i8> @src(<vscale x 2 x i8> %x) {
  %widex = shufflevector <vscale x 2 x i8> %x, <vscale x 2 x i8> poison, <vscale x 4 x i32> zeroinitializer
  %r = add <vscale x 4 x i8> %widex, splat (i8 42)
  ret <vscale x 4 x i8> %r
}

define <vscale x 4 x i8> @tgt(<vscale x 2 x i8> %x) {
  %1 = add <vscale x 2 x i8> %x, splat (i8 42)
  %r = shufflevector <vscale x 2 x i8> %1, <vscale x 2 x i8> poison, <vscale x 4 x i32> zeroinitializer
  ret <vscale x 4 x i8> %r
}
