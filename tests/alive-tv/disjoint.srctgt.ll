define i8 @src(i8 %x, i8 %y) {
  %r = or disjoint i8 %x, %y
  ret i8 %r
}

define i8 @tgt(i8 %x, i8 %y) {
  %r = add nuw i8 %x, %y
  ret i8 %r
}
