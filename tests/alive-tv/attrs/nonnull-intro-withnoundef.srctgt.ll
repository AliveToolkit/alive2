; Introduction of nonnull AND noundef may introduce UB
; ERROR: Source is more defined than target

define i8* @src(i8* %p) {
  ret i8* %p
}

define i8* @tgt(i8* nonnull noundef %p) {
  ret i8* %p
}
