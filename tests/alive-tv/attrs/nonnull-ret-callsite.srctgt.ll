define nonnull ptr @src() {
  %p = call ptr @f()
  ret ptr %p
}

define nonnull ptr @tgt() {
  unreachable
}

; ERROR: Source is more defined than target

declare nonnull ptr @f()
