define i8 @src(i8 %x) {
  %trunc = trunc nsw i8 %x to i1
  %sext = sext i1 %trunc to i8
  ret i8 %sext
}

define i8 @tgt(i8 %x) {
  ret i8 %x
}
