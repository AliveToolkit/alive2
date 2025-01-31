target datalayout = "E-p:64:64:64"
; This optimization is incorrect if x == 0 and y == undef!
; In src, %res cannot be 1 because y cannot be smaller than 0.
; In tgt, %lt and %eq can both be false because y is undef, so %res can be 1.

define i32 @src(i16 noundef %x, i16 noundef %y) {
  %p = alloca i16
  %q = alloca i16
  store i16 %x, ptr %p, align 1
  store i16 %y, ptr %q, align 1
  %res = call i32 @memcmp(ptr %p, ptr %q, i64 2)
  ret i32 %res
}

define i32 @tgt(i16 noundef %x, i16 noundef %y) {
  %lt = icmp ult i16 %x, %y
  %eq = icmp eq i16 %x, %y
  %r = select i1 %eq, i32 0, i32 1
  %res = select i1 %lt, i32 -1, i32 %r
  ret i32 %res
}

declare i32 @memcmp(ptr captures(none), ptr captures(none), i64)
