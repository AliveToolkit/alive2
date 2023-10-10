target datalayout = "e-p:64:64:64"
target triple = "x86_64-unknown-linux-gnu"

define i1 @src(i64 %x, i64 %y, i64 %n) {
  %p = alloca i64
  %q = alloca i64
  store i64 %x, ptr %p
  store i64 %y, ptr %q
  %res = call i32 @memcmp(ptr %p, ptr %q, i64 %n)
  %c = icmp eq i32 %res, 0
  ret i1 %c
}

define i1 @tgt(i64 %x, i64 %y, i64 %n) {
  %p = alloca i64
  %q = alloca i64
  store i64 %x, ptr %p
  store i64 %y, ptr %q
  %res = call i32 @bcmp(ptr %p, ptr %q, i64 %n)
  %c = icmp eq i32 %res, 0
  ret i1 %c
}

declare i32 @bcmp(ptr nocapture, ptr nocapture, i64)
declare i32 @memcmp(ptr nocapture, ptr nocapture, i64)
