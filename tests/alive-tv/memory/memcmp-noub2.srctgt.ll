target datalayout = "e-p:64:64:64"

define i32 @src() {
  %p = alloca i32
  %q = alloca i32
  store i16 257, ptr %p ; 01 01 pp pp
  store i16 257, ptr %q ; 01 01 pp pp
  %res = call i32 @memcmp(ptr %p, ptr %q, i64 4)
  %res2 = add i32 %res, %res
  ret i32 %res2
}

define i32 @tgt() {
  unreachable
}

; ERROR: Source is more defined than target

declare i32 @memcmp(ptr nocapture, ptr nocapture, i64)
