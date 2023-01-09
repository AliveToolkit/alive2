source_filename = "fuzz"
target datalayout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128"
target triple = "aarch64-linux-gnu"

define signext i33 @f(i30 noundef %0) {
  %2 = zext i30 %0 to i61
  %3 = select i1 true, i61 15363, i61 %2
  %4 = call { i50, i1 } @llvm.smul.with.overflow.i50(i50 544532112338943, i50 544532112330751)
  %5 = extractvalue { i50, i1 } %4, 1
  %6 = trunc i61 %3 to i30
  %7 = select i1 %5, i30 522591925, i30 %6
  %8 = sext i30 %7 to i33
  ret i33 %8
}

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare { i50, i1 } @llvm.smul.with.overflow.i50(i50, i50) #0

attributes #0 = { nocallback nofree nosync nounwind speculatable willreturn memory(none) }
