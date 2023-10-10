@A = global i32 0
@B = global i32 0

define i32 @foo() {
  store i32 1, ptr @B
  %l = load i32, ptr @A
  ret i32 %l
}

