; TEST-ARGS:

target datalayout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128"
target triple = "aarch64-unknown-linux-gnu"

define i16 @f(i16 %0, i16 %1, i1 %2) {
  %4 = select i1 %2, i16 %1, i16 %0
  ret i16 %4
}