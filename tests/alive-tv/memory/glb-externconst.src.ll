@x = external constant i8, align 4

define i8 @f() {
  store i8 1, ptr @x
  ret i8 0
}
