define i8 @src() {
  %a = and i8 undef, 1
  %c = icmp eq i8 %a, 0
  br i1 %c, label %then, label %else

then:
  ret i8 0

else:
  ret i8 1
}

define i8 @tgt() {
  ret i8 2
}
