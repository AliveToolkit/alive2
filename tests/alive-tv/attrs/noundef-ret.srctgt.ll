define noundef i32 @src() {
  ret i32 undef
}

define noundef i32 @tgt() {
  unreachable
}
