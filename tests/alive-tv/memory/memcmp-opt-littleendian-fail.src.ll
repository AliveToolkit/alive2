target datalayout = "e-p:64:64:64"
; TEST-ARGS: -disable-undef-input -smt-to=9000
; allowing undef as inputs causes timeout, so it is disabled.

; ex) %x = 0x1020, %y = 0x30
; Note that %x > %y
define i32 @f1(i64 %x, i64 %y) {
  %p = alloca i64
  %q = alloca i64
  store i64 %x, i64* %p, align 1 ; 20 10 00 00 ..
  store i64 %y, i64* %q, align 1 ; 30 00 00 00 ..
  %p8 = bitcast i64* %p to i8*
  %q8 = bitcast i64* %q to i8*
  %res = call i32 @memcmp(i8* %p8, i8* %q8, i64 8) ; %res < 0
  ret i32 %res
}

; ERROR: Value mismatch

declare i32 @memcmp(i8* nocapture, i8* nocapture, i64)

