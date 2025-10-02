define <vscale x 2 x i1> @src(<vscale x 2 x i32> %x) {
  %c1 = icmp eq <vscale x 2 x i32> %x, zeroinitializer
  %c2 = icmp eq <vscale x 2 x i32> %x, splat (i32 42)
  %res = select <vscale x 2 x i1> %c1, <vscale x 2 x i1> splat (i1 true), <vscale x 2 x i1> %c2
  ret <vscale x 2 x i1> %res
}

define <vscale x 2 x i1> @tgt(<vscale x 2 x i32> %x) {
  %c1 = icmp eq <vscale x 2 x i32> %x, zeroinitializer
  %c2 = icmp eq <vscale x 2 x i32> %x, splat (i32 42)
  %res = or <vscale x 2 x i1> %c1, %c2
  ret <vscale x 2 x i1> %res
}
