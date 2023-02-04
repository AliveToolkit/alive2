target datalayout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128"

define i64 @f(i8 %0) {
  %2 = icmp sge i8 -14, %0
  %3 = icmp sgt i1 %2, true
  %4 = sext i1 %3 to i16
  %5 = or i16 -196, %4
  %6 = sext i16 %5 to i64
  %7 = icmp sle i64 0, %6
  %8 = sext i1 %7 to i64
  %9 = sext i1 %2 to i64
  %10 = or i64 %8, %9
  ret i64 %10
}
