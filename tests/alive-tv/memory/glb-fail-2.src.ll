@x = global i32 3, align 4

define i32 @f() {
  %v = load i32, i32* @x
  ret i32 %v
}

; ERROR: Value mismatch
