define <2 x i1> @src(<2 x i4> %x, <2 x i4> %y) {
  %t0 = udiv <2 x i4> <i4 undef, i4 -1>, %x
  %r = icmp uge <2 x i4> %t0, %y
  ret <2 x i1> %r
}

define <2 x i1> @tgt(<2 x i4> %x, <2 x i4> %y) {
  %umul = call { <2 x i4>, <2 x i1> } @llvm.umul.with.overflow.v2i4(<2 x i4> %x, <2 x i4> %y)
  %umul.ov = extractvalue { <2 x i4>, <2 x i1> } %umul, 1
  %umul.not.ov = xor <2 x i1> %umul.ov, <i1 true, i1 true>
  ret <2 x i1> %umul.not.ov
}

declare { <2 x i4>, <2 x i1> } @llvm.umul.with.overflow.v2i4(<2 x i4>, <2 x i4>)
