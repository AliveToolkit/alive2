; TEST-ARGS: -backend-tv --disable-undef-input
; SKIP-IDENTITY
; XFAIL: program doesn't type check!

target datalayout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128"
target triple = "aarch64-unknown-linux-gnu"

define i64 @f(i64 %0, i1 %1) {
  %3 = select i1 %1, i64 %0, i64 689465645428534935
  ret i64 %3
}
