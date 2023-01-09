; ModuleID = 'fuzz'
source_filename = "fuzz"
target datalayout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128"
target triple = "aarch64-linux-gnu"

define i35 @f(i11 zeroext %0, i56 signext %1, i63 zeroext %2, i63 signext %3) {
  %5 = freeze i56 %1
  %6 = trunc i56 %5 to i40
  %7 = trunc i63 %2 to i40
  %8 = call { i40, i1 } @llvm.smul.with.overflow.i40(i40 %6, i40 %7)
  %9 = extractvalue { i40, i1 } %8, 1
  %10 = sext i1 %9 to i40
  %11 = icmp sge i40 %10, 542231669905
  %12 = sext i1 %11 to i52
  %13 = sext i1 %9 to i52
  %14 = sub nuw nsw i52 %12, %13
  %15 = trunc i52 %14 to i35
  ret i35 %15
}

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i37 @llvm.bitreverse.i37(i37) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare { i40, i1 } @llvm.smul.with.overflow.i40(i40, i40) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i3 @llvm.umax.i3(i3, i3) #0

attributes #0 = { nocallback nofree nosync nounwind speculatable willreturn memory(none) }
