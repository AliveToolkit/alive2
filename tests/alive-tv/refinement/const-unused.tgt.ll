@c = constant i32 0
define i32 @f(ptr %p) {
  load i32, ptr @c
  %v = load i32, ptr %p
  ret i32 %v
}
