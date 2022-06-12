; ERROR: Value mismatch

define i8 @src(ptr %p) null_pointer_is_valid {
  store i8 2, ptr null
  store i8 3, ptr %p
  %v = load i8, ptr null
  ret i8 %v
}

define i8 @tgt(ptr %p) null_pointer_is_valid {
  store i8 2, ptr null
  store i8 3, ptr %p
  ret i8 2
}
