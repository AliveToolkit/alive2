; TEST-ARGS: -disable-poison-input

define i8 @src(ptr %p) {
  %v = load i8, ptr %p
  ret i8 0
}

define i8 @tgt(ptr %p) {
  %v = load i8, ptr %p
  %c = icmp eq i8 %v, 0
  br i1 %c, label %then, label %else

then:
  ret i8 0

else:
  ret i8 0
}
