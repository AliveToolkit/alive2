define i16 @src() {
  ret i16 0
}

define i16 @tgt() {
  %a = fptosi half -0.25 to i16
  ret i16 %a
}
