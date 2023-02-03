; TEST-ARGS:
; SKIP-IDENTITY
; XFAIL: Only int types 64 bits or smaller supported for now

target datalayout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128"
target triple = "aarch64-unknown-linux-gnu"

define i64 @f(i128 %0) {
  %2 = trunc i128 %0 to i64
  ret i64 %2
}
