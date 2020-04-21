define <3 x i1> @src(<3 x i8> %x, <3 x i8> %y) {
  %t0 = udiv <3 x i8> <i8 -1, i8 undef, i8 -1>, %x
  %r = icmp uge <3 x i8> %t0, %y
  ret <3 x i1> %r
}

define <3 x i1> @tgt(<3 x i8> %x, <3 x i8> %y) {
  %umul = call { <3 x i8>, <3 x i1> } @llvm.umul.with.overflow.v3i8(<3 x i8> %x, <3 x i8> %y)
  %umul.ov = extractvalue { <3 x i8>, <3 x i1> } %umul, 1
  %umul.not.ov = xor <3 x i1> %umul.ov, <i1 true, i1 true, i1 true>
  ret <3 x i1> %umul.not.ov
}

declare { <3 x i8>, <3 x i1> } @llvm.umul.with.overflow.v3i8(<3 x i8>, <3 x i8>)
