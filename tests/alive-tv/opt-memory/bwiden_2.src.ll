; TEST-ARGS: -dbg

define i32 @f() {
  %a = alloca i32
  %b = alloca i16
  %c = alloca i32
  store i32 1, ptr %a, align 4
  store i16 257, ptr %b, align 2
  store i32 3, ptr %c, align 4
  %x = load i32, ptr %a, align 4
  %y0 = load i16, ptr %b, align 2
  %y = zext i16 %y0 to i32
  %z = load i32, ptr %c, align 4
  %res1 = add i32 %x, %y
  %res2 = add i32 %res1, %z
  ret i32 %res2
}

; CHECK: bits_byte: 16
