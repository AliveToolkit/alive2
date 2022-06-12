; ERROR: Source is more defined

define i8 @src() null_pointer_is_valid {
  %v = load i8, ptr null
  ret i8 %v
}

define i8 @tgt() null_pointer_is_valid {
  unreachable
}
