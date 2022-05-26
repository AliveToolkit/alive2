define i8 @src(i8 %a, ptr %p) {
  %d = load i8, ptr %p
  %x = udiv i8 %a, %d
  %y = udiv i8 %a, %d
  %z = add i8 %x, %y
  ret i8 %z
}

define i8 @tgt(i8 %a, ptr %p) {
  %d = load i8, ptr %p
  %x = udiv i8 %a, %d
  %z = add i8 %x, %x
  ret i8 %z
}
