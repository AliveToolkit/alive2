@x = global i32 0

define i32 @f() {
  store i32 16843009, ptr @x ; 0x01010101
  %v = load i32, ptr @x
  ret i32 %v
}

define i32 @f2() {
  store i32 16843009, ptr @x ; 0x01010101
  %v = load i32, ptr @x
  ret i32 %v
}
