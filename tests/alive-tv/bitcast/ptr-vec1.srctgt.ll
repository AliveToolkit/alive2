define <1 x ptr> @src(ptr %p, ptr %v) {
  store ptr %v, ptr %p
  %l = load <1 x ptr>, ptr %p
  ret <1 x ptr> %l
}

define <1 x ptr> @tgt(ptr %p, ptr %v) {
  store ptr %v, ptr %p
  %l = bitcast ptr %v to <1 x ptr>
  ret <1 x ptr> %l
}
