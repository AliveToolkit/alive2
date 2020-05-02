target datalayout = "e-p:64:64:64"

define i32 @src() {
  %p = alloca i32
	store i32 0, i32* %p
  %p8 = bitcast i32* %p to i8*
  %p8_3 = getelementptr i8, i8* %p8, i64 3
  store i8 1, i8* %p8_3
  %q = getelementptr i8, i8* %p8, i64 1
  %res = call i32 @memcmp(i8* %p8, i8* %q, i64 4) ; noub
	ret i32 %res
}

define i32 @tgt() {
  unreachable
}

; ERROR: Source is more defined than target

declare i32 @memcmp(i8* nocapture, i8* nocapture, i64)
