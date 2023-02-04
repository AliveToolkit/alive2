target datalayout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128"
target triple = "aarch64-linux-gnu"

define i8 @f(i32 %0, i16 %1) {
  %3 = icmp sgt i32 %0, -2
  %4 = icmp eq i16 %1, 0
  %5 = or i1 %4, %3
  %6 = zext i1 %5 to i8
  ret i8 %6
}
