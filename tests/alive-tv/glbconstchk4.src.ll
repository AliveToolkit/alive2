define i32 @f(i32* %p) {
  %v = load i32, i32* %p
  ret i32 %v
}
