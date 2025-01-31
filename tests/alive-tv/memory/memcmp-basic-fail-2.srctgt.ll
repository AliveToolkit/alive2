target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"

define i32 @src() {
  %p = alloca i32
  %q = alloca i32
  store i32 1, ptr %p ; 01 00 00 00
  store i32 2, ptr %q ; 02 00 00 00
  %res = call i32 @memcmp(ptr %p, ptr %q, i64 4)
  ret i32 %res
}

define i32 @tgt() {
	ret i32 1
}

; ERROR: Value mismatch

declare i32 @memcmp(ptr captures(none), ptr captures(none), i64)
