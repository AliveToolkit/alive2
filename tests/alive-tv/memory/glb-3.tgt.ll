@x = global i8 0

define i8 @f() {
  %v = load i8, ptr @x
  %w = load i8, ptr @x
  ret i8 %w
}
