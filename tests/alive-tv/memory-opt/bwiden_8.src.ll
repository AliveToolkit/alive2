; TEST-ARGS: -dbg

define i64 @f() {
  %a = alloca i64
  %b = alloca i64
  %c = alloca i64
  store i64 1, i64* %a, align 8
  store i64 2, i64* %b, align 8
  store i64 3, i64* %c, align 8
  %x = load i64, i64* %a, align 8
  %y = load i64, i64* %b, align 8
  %z = load i64, i64* %c, align 8
  %res1 = add i64 %x, %y
  %res2 = add i64 %res1, %z
  ret i64 %res2
}

; CHECK: bits_byte: 64
