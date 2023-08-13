define nonnull ptr @src(ptr %p) {
  ret ptr null
}

define nonnull ptr @tgt(ptr %p) {
  ret ptr poison
}
