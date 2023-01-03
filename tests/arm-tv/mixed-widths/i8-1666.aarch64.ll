; TEST-ARGS: --disable-undef-input

target datalayout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128"
target triple = "aarch64-unknown-linux-gnu"

define i8 @f(i8 %0, i1 %1) {
  %3 = select i1 %1, i8 %0, i8 127
  ret i8 %3
}
