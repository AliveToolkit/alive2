; A range attribute on a parameter must be honored: %x can only be 1.
define i8 @src(i8 noundef range(i8 1, 2) %x) {
  ret i8 %x
}

define i8 @tgt(i8 noundef range(i8 1, 2) %x) {
  ret i8 1
}
