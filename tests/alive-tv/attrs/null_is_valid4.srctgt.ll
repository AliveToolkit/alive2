define i8 @src() null_pointer_is_valid {
  %V = load i8, ptr null
  ret i8 %V
}

define i8 @tgt() null_pointer_is_valid memory(read, inaccessiblemem: none) {
  %V = load i8, ptr null
  ret i8 %V
}
