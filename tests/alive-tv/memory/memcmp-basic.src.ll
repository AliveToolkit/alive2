target datalayout = "e-p:64:64:64"

define i32 @lt() {
  %p = alloca i32
  %q = alloca i32
  store i32 1, i32* %p ; 01 00 00 00
  store i32 2, i32* %q ; 02 00 00 00
  %p8 = bitcast i32* %p to i8*
  %q8 = bitcast i32* %q to i8*
  %res = call i32 @memcmp(i8* %p8, i8* %q8, i64 4)
  ret i32 %res
}

define i32 @lt2() {
  %p = alloca i32
  %q = alloca i32
  store i32 1, i32* %p   ; 01 00 00 00
  store i32 513, i32* %q ; 01 02 00 00
  %p8 = bitcast i32* %p to i8*
  %q8 = bitcast i32* %q to i8*
  %res = call i32 @memcmp(i8* %p8, i8* %q8, i64 4)
  ret i32 %res
}

define i32 @lt3() {
  %p = alloca i32
  %q = alloca i32
  store i32 513, i32* %p    ; 01 02 00 00
  store i32 197121, i32* %q ; 01 02 03 00
  %p8 = bitcast i32* %p to i8*
  %q8 = bitcast i32* %q to i8*
  %res = call i32 @memcmp(i8* %p8, i8* %q8, i64 4)
  ret i32 %res
}

define i32 @lt4() {
  %p = alloca i32
  %q = alloca i32
  store i32 197121, i32* %p   ; 01 02 03 00
  store i32 67305985, i32* %q ; 01 02 03 04
  %p8 = bitcast i32* %p to i8*
  %q8 = bitcast i32* %q to i8*
  %res = call i32 @memcmp(i8* %p8, i8* %q8, i64 4)
  ret i32 %res
}

define i32 @eq() {
  %p = alloca i32
  %q = alloca i32
  store i32 10, i32* %p
  store i32 10, i32* %q
  %p8 = bitcast i32* %p to i8*
  %q8 = bitcast i32* %q to i8*
  %res = call i32 @memcmp(i8* %p8, i8* %q8, i64 4)
  ret i32 %res
}

define i32 @gt() {
  %p = alloca i32
  %q = alloca i32
  store i32 2, i32* %p ; 02 00 00 00
  store i32 1, i32* %q ; 01 00 00 00
  %p8 = bitcast i32* %p to i8*
  %q8 = bitcast i32* %q to i8*
  %res = call i32 @memcmp(i8* %p8, i8* %q8, i64 4)
  ret i32 %res
}

define i32 @gt2() {
  %p = alloca i32
  %q = alloca i32
  store i32 67305985, i32* %p ; 01 02 03 04
  store i32 197121, i32* %q   ; 01 02 03 00
  %p8 = bitcast i32* %p to i8*
  %q8 = bitcast i32* %q to i8*
  %res = call i32 @memcmp(i8* %p8, i8* %q8, i64 4)
  ret i32 %res
}

declare i32 @memcmp(i8* nocapture, i8* nocapture, i64)

