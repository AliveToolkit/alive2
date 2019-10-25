target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"

define i32 @lt() {
  %p = alloca i8
  %q = alloca i8
  store i8 1, i8* %p
  store i8 2, i8* %q
  %res = call i32 @memcmp(i8* %p, i8* %q, i64 1)
  ret i32 %res
}

; ERROR: Value mismatch

declare i32 @memcmp(i8* nocapture, i8* nocapture, i64)

