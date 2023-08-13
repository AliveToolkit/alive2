define void @src(ptr dereferenceable_or_null(4) nonnull %p) {
  ret void
}

define void @tgt(ptr dereferenceable_or_null(4) nonnull %p) {
  load i32, ptr %p, align 1
  ret void
}
