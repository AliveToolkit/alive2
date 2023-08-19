@g = global i8 0

define i32 @f() {
  %v = load i8, ptr @g
  %w = zext i8 %v to i32
  ret i32 %w
}
