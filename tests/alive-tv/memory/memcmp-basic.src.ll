target datalayout = "e-p:64:64:64"

define i32 @lt() {
  %p = alloca i32
  %q = alloca i32
  store i32 1, ptr %p ; 01 00 00 00
  store i32 2, ptr %q ; 02 00 00 00
  %res = call i32 @memcmp(ptr %p, ptr %q, i64 4)
  ret i32 %res
}

define i32 @lt2() {
  %p = alloca i32
  %q = alloca i32
  store i32 1, ptr %p   ; 01 00 00 00
  store i32 513, ptr %q ; 01 02 00 00
  %res = call i32 @memcmp(ptr %p, ptr %q, i64 4)
  ret i32 %res
}

define i32 @lt3() {
  %p = alloca i32
  %q = alloca i32
  store i32 513, ptr %p    ; 01 02 00 00
  store i32 197121, ptr %q ; 01 02 03 00
  %res = call i32 @memcmp(ptr %p, ptr %q, i64 4)
  ret i32 %res
}

define i32 @lt4() {
  %p = alloca i32
  %q = alloca i32
  store i32 197121, ptr %p   ; 01 02 03 00
  store i32 67305985, ptr %q ; 01 02 03 04
  %res = call i32 @memcmp(ptr %p, ptr %q, i64 4)
  ret i32 %res
}

define i32 @eq() {
  %p = alloca i32
  %q = alloca i32
  store i32 10, ptr %p
  store i32 10, ptr %q
  %res = call i32 @memcmp(ptr %p, ptr %q, i64 4)
  ret i32 %res
}

define i32 @gt() {
  %p = alloca i32
  %q = alloca i32
  store i32 2, ptr %p ; 02 00 00 00
  store i32 1, ptr %q ; 01 00 00 00
  %res = call i32 @memcmp(ptr %p, ptr %q, i64 4)
  ret i32 %res
}

define i32 @gt2() {
  %p = alloca i32
  %q = alloca i32
  store i32 67305985, ptr %p ; 01 02 03 04
  store i32 197121, ptr %q   ; 01 02 03 00
  %res = call i32 @memcmp(ptr %p, ptr %q, i64 4)
  ret i32 %res
}

declare i32 @memcmp(ptr captures(none), ptr captures(none), i64)
