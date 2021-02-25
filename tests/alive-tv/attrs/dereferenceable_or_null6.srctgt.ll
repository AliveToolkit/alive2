define void @src(i32* dereferenceable_or_null(4) nonnull %p) {
  ret void
}

define void @tgt(i32* dereferenceable_or_null(4) nonnull %p) {
  load i32, i32* %p, align 1
  ret void
}
