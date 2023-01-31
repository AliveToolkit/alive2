target datalayout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128"
target triple = "aarch64-linux-gnu"

define i46 @f() {
  %1 = freeze i46 poison
  %2 = icmp sgt i46 0, %1
  %3 = zext i1 %2 to i46
  %4 = xor i46 %3, 1
  %5 = trunc i46 %4 to i1
  %6 = select i1 %5, i46 0, i46 %1
  ret i46 %6
}
