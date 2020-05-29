; TEST-ARGS: -skip-smt

define i8 @src(i8 %x, i8 %y) {
  %v = add i8 %x, %y
  ret i8 %v
}

define i8 @tgt(i8 %x, i8 %y) {
  %v = add i8 %y, %x
  ret i8 %v
}

; ERROR: Skip
