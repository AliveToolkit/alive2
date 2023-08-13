define float @src(ptr dereferenceable(15) %p) {
  %r = load float, ptr %p, align 16
  ret float %r
}

define float @tgt(ptr dereferenceable(15) %p) {
  %bc = getelementptr inbounds <4 x float>, ptr %p, i64 0, i64 0
  %r = load float, ptr %bc, align 16
  ret float %r
}
