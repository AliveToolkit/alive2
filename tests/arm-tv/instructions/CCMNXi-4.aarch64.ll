target datalayout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128"
target triple = "aarch64-linux-gnu"

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare { i41, i1 } @llvm.smul.with.overflow.i41(i41, i41) #0

define i41 @f() {
  %1 = call { i41, i1 } @llvm.smul.with.overflow.i41(i41 1086593171964, i41 -1086593171965)
  %2 = extractvalue { i41, i1 } %1, 1
  %3 = select i1 false, i41 1086593171960, i41 0
  %4 = select i1 %2, i41 0, i41 1
  ret i41 %4
}

attributes #0 = { nocallback nofree nosync nounwind speculatable willreturn memory(none) }
