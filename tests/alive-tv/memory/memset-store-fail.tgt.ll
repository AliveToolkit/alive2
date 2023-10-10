@x = global i32 0

define i32 @f() {
  store i32 0, ptr @x
  %v = load i32, ptr @x
  ret i32 %v
}
