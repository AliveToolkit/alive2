target datalayout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128"
target triple = "aarch64-linux-gnu"

define i16 @f(i8 %0) {
  %2 = icmp slt i8 %0, 0
  %3 = icmp slt i8 %0, -16
  %4 = and i1 %2, %3
  %5 = zext i1 %4 to i16
  ret i16 %5
}
