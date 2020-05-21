target datalayout = "e-p:64:64:64"

define i32 @src() {
  %p = alloca i32
  %p8 = bitcast i32* %p to i8*
  store i8 0, i8* %p8
  %res = call i32 @memcmp(i8* %p8, i8* %p8, i64 4) ; poison
  ret i32 %res
}

define i32 @tgt() {
  unreachable
}

; ERROR: Source is more defined than target

declare i32 @memcmp(i8* nocapture, i8* nocapture, i64)
