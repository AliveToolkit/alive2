; TEST-ARGS: -dbg

define i32 @f() {
  %a = alloca i32
  %b = alloca i32, align 8
  %c = alloca i32
  store i32 1, i32* %a, align 4
  store i32 2, i32* %b, align 8
  store i32 3, i32* %c, align 4
  %x = load i32, i32* %a, align 4
  %y = load i32, i32* %b, align 4
  %z = load i32, i32* %c, align 4
  %res1 = add i32 %x, %y
  %res2 = add i32 %res1, %z
  ret i32 %res2
}

; CHECK: bits_byte: 32
