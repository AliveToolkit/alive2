target datalayout = "E-p:64:64:64"
; TEST-ARGS: -disable-undef-input -smt-to=10000
; allowing undef as inputs causes timeout, so it is disabled.

define i32 @f1(i64 %x, i64 %y) {
  %p = alloca i64
  %q = alloca i64
  store i64 %x, i64* %p, align 1
  store i64 %y, i64* %q, align 1
  %p8 = bitcast i64* %p to i8*
  %q8 = bitcast i64* %q to i8*
  %res = call i32 @memcmp(i8* %p8, i8* %q8, i64 8)
  ret i32 %res
}

declare i32 @memcmp(i8* nocapture, i8* nocapture, i64)

