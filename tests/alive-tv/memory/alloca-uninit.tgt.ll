define i32 @f() {
  %p = alloca i32
  %v = load i32, i32* %p
  ret i32 %v
}
