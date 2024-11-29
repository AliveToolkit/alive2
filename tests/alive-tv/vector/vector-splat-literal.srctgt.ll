define <2 x i8> @src(<2 x i8> %x) {
  %rem.i = srem <2 x i8> %x, splat(i8 2)
  %cmp.i = icmp slt <2 x i8> %rem.i, zeroinitializer
  %add.i = select <2 x i1> %cmp.i, <2 x i8> splat(i8 2), <2 x i8> zeroinitializer
  ret <2 x i8> %add.i
}

define <2 x i8> @tgt(<2 x i8> %x) {
  %rem.i = srem <2 x i8> %x, splat(i8 2)
  %tmp1 = and <2 x i8> %rem.i, splat(i8 2)
  ret <2 x i8> %tmp1
}
