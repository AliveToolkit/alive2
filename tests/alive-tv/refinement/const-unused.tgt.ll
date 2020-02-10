@c = constant i32 0
define i32 @f(i32* %p) {
  load i32, i32* @c
  %v = load i32, i32* %p
  ret i32 %v
}
