@x = internal constant i32 0
@y = internal constant i32 1

define void @f() {
  %a = load i32, ptr @x
  %b = load i32, ptr @y
  ret void
}

; ERROR: Mismatch in memory
