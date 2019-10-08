@x = constant i8 0

define i8 @f() {
  store i8 1, i8* @x
  ret i8 0
}
