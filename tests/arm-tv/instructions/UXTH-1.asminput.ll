source_filename = "fuzz"
target datalayout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128"
target triple = "aarch64-linux-gnu"

define zeroext i16 @f(i16 signext %0, i16 zeroext %1, i16 zeroext %2, i16 %3, i16 noundef %4) {
  %6 = freeze i16 %0
  ret i16 %6
}

