define void @src(ptr dereferenceable(1) %p) {
  ret void
}

define void @tgt(ptr readnone dereferenceable(1) %p) {
  ret void
}
