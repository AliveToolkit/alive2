define i16 @src() {
  ret i16 0
}

define i16 @tgt() {
  %a = fptoui half -0.25 to i16
  ret i16 %a
}
