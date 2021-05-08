; ERROR: Target is more poisonous than source

define i8 @src() {
  %i = fptosi float -0.0 to i8
  ret i8 %i
}

define i8 @tgt() {
  ret i8 poison
}
