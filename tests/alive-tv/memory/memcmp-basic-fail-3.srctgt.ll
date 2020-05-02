target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"

define i32 @src() {
  %p = alloca i32
  %q = alloca i32
  store i32 2, i32* %p ; 02 00 00 00
  store i32 1, i32* %q ; 01 00 00 00
  %p8 = bitcast i32* %p to i8*
  %q8 = bitcast i32* %q to i8*
  %res = call i32 @memcmp(i8* %p8, i8* %q8, i64 4)
  ret i32 %res
}

define i32 @tgt() {
  ret i32 0
}

; ERROR: Value mismatch

declare i32 @memcmp(i8* nocapture, i8* nocapture, i64)
