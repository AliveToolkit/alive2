@A = global i32 0
@B = global i32 0

define i32 @foo() {
  %l = load i32, ptr @A
  store i32 1, ptr @B
  ret i32 %l
}

