target datalayout = "e-p:64:64:64"
target triple = "x86_64-unknown-linux-gnu"

define i32 @src(ptr %p, i64 %n) {
  %res = call i32 @bcmp(ptr %p, ptr %p, i64 %n)
  ret i32 %res
}

define i32 @tgt(ptr %p, i64 %n) {
  ret i32 0
}

declare i32 @bcmp(ptr captures(none), ptr captures(none), i64)
