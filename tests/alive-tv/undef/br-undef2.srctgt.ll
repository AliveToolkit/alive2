define i8 @src(i1 %c) {
  br i1 %c, label %a, label %b

a:
  ret i8 1

b:
  ret i8 0
}

define i8 @tgt(i1 %c) {
  %ext = zext i1 %c to i8
  ret i8 %ext
}
