define i8 @src(i8* %p) {
  %v = load i8, i8* %p
  ret i8 %v
}

define i8 @tgt(i8* readonly %p) {
  %v = load i8, i8* %p
  ret i8 %v
}
