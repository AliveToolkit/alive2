define i8 @src(i8 %x) {
  %trunc = trunc nuw i8 %x to i1
  %zext = zext i1 %trunc to i8
  ret i8 %zext
}

define i8 @tgt(i8 %x) {
  ret i8 %x
}
