@A = global i32 0
@B = global i32 0

define i32 @foo() {
  store i32 1, i32* @B
  %l = load i32, i32* @A
  ret i32 %l
}

