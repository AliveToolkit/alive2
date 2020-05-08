target datalayout = "e-p:64:64:64"
target triple = "x86_64-unknown-linux-gnu"

define i32 @src(i8 *%p, i64 %n) {
  %res = call i32 @memcmp(i8* %p, i8* %p, i64 %n)
  ret i32 %res
}

define i32 @tgt(i8 *%p, i64 %n) {
  ret i32 0
}

declare i32 @memcmp(i8* nocapture, i8* nocapture, i64)
