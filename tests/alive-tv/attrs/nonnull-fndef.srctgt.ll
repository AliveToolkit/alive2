define nonnull ptr @src(ptr nonnull %p) {
  ret ptr %p
}

define nonnull ptr @tgt(ptr nonnull %p) {
  unreachable
}

; ERROR: Source is more defined than target
