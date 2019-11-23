define i16 @f() {
  %p = alloca i16
  %v = load i16, i16* %p
  %res0 = add i16 %v, %v
  %res = add i16 %res0, 1
  ret i16 %res
}
