; ERROR: Source is more defined than target

define i8 @src(ptr %p) {
  store i8 3, ptr %p
  ret i8 2
}

define i8 @tgt(ptr readonly %p) {
  store i8 3, ptr %p
  ret i8 2
}
