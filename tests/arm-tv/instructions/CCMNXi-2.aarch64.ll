source_filename = "fuzz"
target datalayout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128"
target triple = "aarch64-linux-gnu"

define signext i9 @f(i41 noundef zeroext %0, i64 noundef %1) {
  %3 = call { i34, i1 } @llvm.smul.with.overflow.i34(i34 5719873650, i34 5719873652)
  %4 = extractvalue { i34, i1 } %3, 1
  %5 = icmp eq i1 %4, true
  %6 = zext i1 %5 to i9
  ret i9 %6
}

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare { i34, i1 } @llvm.smul.with.overflow.i34(i34, i34) #0

attributes #0 = { nocallback nofree nosync nounwind speculatable willreturn memory(none) }
