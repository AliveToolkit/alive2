@x = constant i32 5, align 4

define i32 @f() {
  %v = load i32, ptr @x
  ret i32 %v
}
