define i8 @f() {
  %ptr = alloca i8
  store i8 20, i8* %ptr
  %v = load i8, i8* %ptr
  ret i8 %v
}
