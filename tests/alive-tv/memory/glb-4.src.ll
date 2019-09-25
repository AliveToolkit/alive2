@x = global i8 0
@y = global i8 0

define i8 @f() {
  %v = load i8, i8* @x
  ret i8 %v
}
