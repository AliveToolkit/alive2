define nonnull ptr @src(ptr %p) {
  ret ptr null
}

define nonnull ptr @tgt(ptr %p) {
  ; nonnull null is poison, not unreachable
  unreachable
}

; ERROR: Source is more defined than target
