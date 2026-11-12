define i24 @src() {
  %a = extractvalue [5 x i24] [i24 0, i24 1, i24 2, i24 3, i24 4], 4
  ret i24 %a
}

define i24 @tgt() {
  ret i24 4
}
