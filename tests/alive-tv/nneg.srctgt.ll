define i16 @src(i8 %x) {
  %r = zext nneg i8 %x to i16
  ret i16 %r
}

define i16 @tgt(i8 %x) {
  %r = sext i8 %x to i16
  ret i16 %r
}
