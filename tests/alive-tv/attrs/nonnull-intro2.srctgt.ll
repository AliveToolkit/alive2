define void @src(ptr %p) {
  call void @g(ptr %p)
  ret void
}

define void @tgt(ptr nonnull %p) {
  call void @g(ptr %p)
  ret void
}

declare void @g(ptr dereferenceable(4))
