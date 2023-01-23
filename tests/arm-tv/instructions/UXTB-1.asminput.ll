source_filename = "fuzz"
target datalayout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128"
target triple = "aarch64-linux-gnu"

define zeroext i8 @f(i8 signext %0, i8 zeroext %1, i8 zeroext %2, i8 %3, i8 noundef %4) {
  %6 = freeze i8 %0
  ret i8 %6
}

