target datalayout = "e-p:64:64:64"

define i32 @src() {
  %p = alloca i32
	store i32 0, ptr %p
  %p8_3 = getelementptr i8, ptr %p, i64 3
  store i8 1, ptr %p8_3
  %q = getelementptr i8, ptr %p, i64 1
  %res = call i32 @memcmp(ptr %p, ptr %q, i64 3) ; noub
	ret i32 %res
}

define i32 @tgt() {
  unreachable
}

; ERROR: Source is more defined than target

declare i32 @memcmp(ptr nocapture, ptr nocapture, i64)
