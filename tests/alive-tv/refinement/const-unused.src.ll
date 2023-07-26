define i32 @f(ptr %p) {
  %v = load i32, ptr %p
  ret i32 %v
}
