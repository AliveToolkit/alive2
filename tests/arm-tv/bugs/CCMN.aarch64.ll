target datalayout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128"

define i8 @f(i8 %0) {
  %2 = icmp sge i8 %0, -2
  %3 = select i1 %2, i16 -1, i16 0
  %4 = trunc i16 %3 to i11
  %5 = icmp sgt i11 %4, 0
  %6 = icmp sgt i1 %2, %5
  %7 = zext i1 %6 to i8
  ret i8 %7
}
