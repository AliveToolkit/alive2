define i8 @src() {
  br i1 undef, label %a, label %b

a:
  ret i8 0

b:
  ret i8 3
}

define i8 @tgt() {
  unreachable
}
