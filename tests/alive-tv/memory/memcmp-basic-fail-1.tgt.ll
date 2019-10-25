target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"

define i32 @lt() {
  ret i32 0
}

; ERROR: Value mismatch

declare i32 @memcmp(i8* nocapture, i8* nocapture, i64)

