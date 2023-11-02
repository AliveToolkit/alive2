target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"

define i32 @src() {
  %p = alloca i8
  %q = alloca i8
  store i8 1, ptr %p
  store i8 2, ptr %q
  %res = call i32 @memcmp(ptr %p, ptr %q, i64 1)
  ret i32 %res
}

define i32 @tgt() {
	ret i32 0
}

; ERROR: Value mismatch

declare i32 @memcmp(ptr nocapture, ptr nocapture, i64)

