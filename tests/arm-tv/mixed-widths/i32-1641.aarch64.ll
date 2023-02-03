; TEST-ARGS:

target datalayout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128"
target triple = "aarch64-unknown-linux-gnu"

define i32 @f(i16 %0) {
  %2 = zext i16 %0 to i32
  ret i32 %2
}
