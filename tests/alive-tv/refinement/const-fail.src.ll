@x = internal constant i32 0
@y = internal constant i32 1

define void @f() {
  %a = load i32, i32* @x
  %b = load i32, i32* @y
  ret void
}

; ERROR: Mismatch in memory
