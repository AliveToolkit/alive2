@x = global i32 0

define void @f() {
  store i32 10, i32* @x
  ret void
}

; ERROR: Mismatch in memory
