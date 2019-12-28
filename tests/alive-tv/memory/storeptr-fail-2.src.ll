@x = global i8* null
@y = global i8 0

define void @f1() {
  store i8* @y, i8** @x
  ret void
}

; ERROR: Mismatch in memory
