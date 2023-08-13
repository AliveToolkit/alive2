@g = constant i32 0
define i32 @f() {
  %x = load i32, ptr @g
  ret i32 %x
}
