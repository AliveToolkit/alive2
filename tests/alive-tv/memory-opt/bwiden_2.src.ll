; TEST-ARGS: -dbg

define i32 @f() {
  %a = alloca i32
  %b = alloca i16
  %c = alloca i32
  store i32 1, i32* %a
  store i16 257, i16* %b
  store i32 3, i32* %c
  %x = load i32, i32* %a
  %y0 = load i16, i16* %b
  %y = zext i16 %y0 to i32
  %z = load i32, i32* %c
  %res1 = add i32 %x, %y
  %res2 = add i32 %res1, %z
  ret i32 %res2
}

; CHECK: bits_byte: 16
