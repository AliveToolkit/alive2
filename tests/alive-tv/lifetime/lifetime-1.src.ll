define void @f1() {
  %p = alloca i32
  ret void
}

define i32 @f2() {
  %p = alloca i32
  store i32 10, ptr %p
  %v = load i32, ptr %p
  ret i32 %v
}
