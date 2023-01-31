target datalayout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128"

define i16 @f(i16 %0, i64 %1) {
  %3 = call i64 @llvm.sshl.sat.i64(i64 %1, i64 -6940707247461790583)
  %4 = freeze i64 %3
  %5 = trunc i64 %4 to i16
  %6 = urem i16 %5, %0
  %7 = ashr i16 %6, 1
  ret i16 %7
}

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i64 @llvm.sshl.sat.i64(i64, i64) #0

attributes #0 = { nocallback nofree nosync nounwind speculatable willreturn memory(none) }
