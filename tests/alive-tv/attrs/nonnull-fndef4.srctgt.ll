define nonnull noundef ptr @src(ptr %p) {
  ret ptr null
}

define nonnull noundef ptr @tgt(ptr %p) {
  unreachable
}
