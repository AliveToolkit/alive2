define i16 @f() {
  %p = alloca i16
  %v = load i16, i16* %p
  ret i16 %v
}
