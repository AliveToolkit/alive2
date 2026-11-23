define <2 x ptr> @src(<2 x ptr> %a, <2 x ptr> %b) {
  %i = call <4 x ptr> @llvm.vector.interleave2.v4p0(<2 x ptr> %a, <2 x ptr> %b)
  %d = call { <2 x ptr>, <2 x ptr> } @llvm.vector.deinterleave2.v4p0(<4 x ptr> %i)
  %r = extractvalue { <2 x ptr>, <2 x ptr> } %d, 0
  ret <2 x ptr> %r
}

define <2 x ptr> @tgt(<2 x ptr> %a, <2 x ptr> %b) {
  ret <2 x ptr> %a
}
