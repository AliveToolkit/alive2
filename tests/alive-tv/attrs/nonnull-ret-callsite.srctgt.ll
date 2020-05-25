define nonnull i8* @src() {
  %p = call i8* @f()
  ret i8* %p
}

define nonnull i8* @tgt() {
  unreachable
}

; ERROR: Source is more defined than target

declare nonnull i8* @f()
