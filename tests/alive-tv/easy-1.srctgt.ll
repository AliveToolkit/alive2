define i8 @src(i8, i8) {
  %x = add nsw i8 %0, %1
  ret i8 %x
}

define i8 @tgt(i8, i8) {
  %x = add i8 %0, %1
  ret i8 %x
}
