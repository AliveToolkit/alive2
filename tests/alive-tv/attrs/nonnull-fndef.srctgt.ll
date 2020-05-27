define nonnull i8* @src(i8* nonnull %p) {
  ret i8* %p
}

define nonnull i8* @tgt(i8* nonnull %p) {
  unreachable
}

; ERROR: Source is more defined than target
