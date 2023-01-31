target datalayout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128"

define i8 @f() {
  %1 = freeze i8 poison
  %2 = call i8 @llvm.fshr.i8(i8 1, i8 1, i8 %1)
  ret i8 %2
}

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i8 @llvm.fshr.i8(i8, i8, i8) #0

attributes #0 = { nocallback nofree nosync nounwind speculatable willreturn memory(none) }
