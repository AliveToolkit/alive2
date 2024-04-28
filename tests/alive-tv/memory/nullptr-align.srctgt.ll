; ERROR: Source is more defined than target

define i8 @src() null_pointer_is_valid {
  %v = load i8, ptr null, align 4
  ret i8 %v
}

define i8 @tgt() null_pointer_is_valid {
  %v = load i8, ptr null, align 8
  ret i8 %v
}
