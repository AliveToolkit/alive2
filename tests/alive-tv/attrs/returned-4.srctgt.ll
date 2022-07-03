define <2 x i4> @src(<2 x i4> %0) {
  ret <2 x i4> %0
}

define <2 x i4> @tgt(<2 x i4> returned %0) {
  ret <2 x i4> %0
}
