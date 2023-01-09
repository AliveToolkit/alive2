source_filename = "fuzz"
target datalayout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128"
target triple = "aarch64-linux-gnu"

define zeroext i35 @f(i55 zeroext %0, i54 %1, i59 zeroext %2, i26 zeroext %3) {
  %5 = trunc i26 %3 to i13
  %6 = trunc i59 %2 to i13
  %7 = trunc i59 %2 to i1
  %8 = select i1 %7, i13 %5, i13 %6
  %9 = trunc i59 %2 to i55
  %10 = call { i55, i1 } @llvm.umul.with.overflow.i55(i55 4503599627370495, i55 %9)
  %11 = extractvalue { i55, i1 } %10, 1
  %12 = trunc i13 %8 to i8
  %13 = freeze i1 %11
  %14 = zext i1 %13 to i8
  %15 = trunc i59 %2 to i1
  %16 = select i1 %15, i8 %12, i8 %14
  %17 = zext i1 %11 to i64
  %18 = zext i8 %16 to i64
  %19 = add nuw nsw i64 %17, %18
  %20 = trunc i64 %19 to i35
  ret i35 %20
}

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare { i55, i1 } @llvm.umul.with.overflow.i55(i55, i55) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i47 @llvm.abs.i47(i47, i1 immarg) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i5 @llvm.ctlz.i5(i5, i1 immarg) #0

attributes #0 = { nocallback nofree nosync nounwind speculatable willreturn memory(none) }
