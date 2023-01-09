source_filename = "fuzz"
target datalayout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128"
target triple = "aarch64-linux-gnu"

define noundef zeroext i15 @f(i15 zeroext %0, i15 %1) {
  %3 = call i15 @llvm.fshr.i15(i15 %0, i15 %0, i15 -513)
  %4 = call i15 @llvm.fshl.i15(i15 %3, i15 %3, i15 -4609)
  ret i15 %4
}

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i15 @llvm.fshr.i15(i15, i15, i15) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i15 @llvm.fshl.i15(i15, i15, i15) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i15 @llvm.ushl.sat.i15(i15, i15) #0

attributes #0 = { nocallback nofree nosync nounwind speculatable willreturn memory(none) }
