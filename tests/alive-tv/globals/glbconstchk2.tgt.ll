@g = global i32 0

define i32 @f() {
  %v = load i32, ptr @g
  ret i32 %v
}
