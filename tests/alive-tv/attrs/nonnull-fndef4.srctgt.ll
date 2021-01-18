define nonnull noundef i8* @src(i8* %p) {
  ret i8* null
}

define nonnull noundef i8* @tgt(i8* %p) {
  unreachable
}
