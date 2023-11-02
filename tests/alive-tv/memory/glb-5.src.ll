@x = constant i8 0

define i8 @f() {
  store i8 1, ptr @x
  ret i8 0
}
