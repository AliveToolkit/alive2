@g = constant i32 0
define i32 @f() {
  %x = load i32, i32* @g
  ret i32 %x
}
