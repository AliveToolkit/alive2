target datalayout = "e-p:64:64:64"

; ex) %x = 0x1020, %y = 0x30
; Note that %x > %y
define i32 @src(i64 %x, i64 %y) {
  %p = alloca i64
  %q = alloca i64
  store i64 %x, ptr %p, align 1 ; 20 10 00 00 ..
  store i64 %y, ptr %q, align 1 ; 30 00 00 00 ..
  %res = call i32 @memcmp(ptr %p, ptr %q, i64 8) ; %res < 0
  ret i32 %res
}

define i32 @tgt(i64 %x, i64 %y) {
  %lt = icmp ult i64 %x, %y
  %eq = icmp eq i64 %x, %y
  %r = select i1 %eq, i32 0, i32 1
  %res = select i1 %lt, i32 -1, i32 %r
  ret i32 %res
}

; ERROR: Value mismatch

declare i32 @memcmp(ptr captures(none), ptr captures(none), i64)
