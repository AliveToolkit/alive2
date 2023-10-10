@x = global i32 0, align 4

define i32 @f() {
  %i = ptrtoint ptr @x to i32
  %x = urem i32 %i, 4
  ret i32 %x
}
