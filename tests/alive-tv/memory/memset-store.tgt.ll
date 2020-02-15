declare void @llvm.memset.p0i8.i8(i8*, i8, i32, i1)

@x = global i32 0

define i32 @f() {
  store i32 16843009, i32* @x ; 0x01010101
  %v = load i32, i32* @x
  ret i32 %v
}

define i32 @f2() {
  store i32 16843009, i32* @x ; 0x01010101
  %v = load i32, i32* @x
  ret i32 %v
}
