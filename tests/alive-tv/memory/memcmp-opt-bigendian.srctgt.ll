target datalayout = "E-p:64:64:64"
; TEST-ARGS: -disable-undef-input
; This optimization is incorrect if x == 0 and y == undef!
; In src, %res cannot be 1 because y cannot be smaller than 0.
; In tgt, %lt and %eq can both be false because y is undef, so %res can be 1.

define i32 @src(i16 %x, i16 %y) {
  %p = alloca i16
  %q = alloca i16
  store i16 %x, i16* %p, align 1
  store i16 %y, i16* %q, align 1
  %p8 = bitcast i16* %p to i8*
  %q8 = bitcast i16* %q to i8*
  %res = call i32 @memcmp(i8* %p8, i8* %q8, i64 2)
  ret i32 %res
}

define i32 @tgt(i16 %x, i16 %y) {
  %lt = icmp ult i16 %x, %y
  %eq = icmp eq i16 %x, %y
  %r = select i1 %eq, i32 0, i32 1
  %res = select i1 %lt, i32 -1, i32 %r
  ret i32 %res
}

declare i32 @memcmp(i8* nocapture, i8* nocapture, i64)	
