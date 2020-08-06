define noundef i32 @src(i32 %x) {
  ret i32 %x
}

define noundef i32 @tgt(i32 %x) {
  unreachable
}

; ERROR: Source is more defined than target
