; Without noundef, a value outside the range is poison, which 1 refines.
define i8 @src(i8 range(i8 1, 2) %x) {
  ret i8 %x
}

define i8 @tgt(i8 range(i8 1, 2) %x) {
  ret i8 1
}
