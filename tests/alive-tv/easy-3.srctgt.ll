; ERROR: Target is more poisonous than source

define i8 @src(i8, i8) {
  %x = add nsw i8 %0, %1
  ret i8 %x
}

define i8 @tgt(i8, i8) {
  %x = add nuw i8 %0, %1
  ret i8 %x
}
