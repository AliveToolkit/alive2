; ERROR: Source is more defined than target

define i8 @src(i8* %p) {
  store i8 3, i8* %p
  ret i8 2
}

define i8 @tgt(i8* readonly %p) {
  store i8 3, i8* %p
  ret i8 2
}
