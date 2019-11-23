@x = constant i32 5, align 4

define i32 @f() {
  %v = load i32, i32* @x
  ret i32 %v
}

; XFAIL: Mismatch in memory
