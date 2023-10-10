target datalayout = "e-p:64:64:64"

define i32 @src(ptr %p, ptr %q) {
  ret i32 0
}

define i32 @tgt(ptr %p, ptr %q) {
  %res = call i32 @memcmp(ptr %p, ptr %q, i64 0)
  ret i32 %res
}

declare i32 @memcmp(ptr nocapture, ptr nocapture, i64)
