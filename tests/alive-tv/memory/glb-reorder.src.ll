@A = global i32 0
@B = global i32 0

define i32 @foo() {
  %l = load i32, i32* @A
  store i32 1, i32* @B
  ret i32 %l
}

