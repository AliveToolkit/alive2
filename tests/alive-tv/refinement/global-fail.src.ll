@x = global i32 0

define void @f() {
  store i32 10, ptr @x
  ret void
}

; ERROR: Mismatch in memory
