@x = global ptr null
@y = global i8 0

define void @f1() {
  store ptr @y, ptr @x
  ret void
}

; ERROR: Mismatch in memory
