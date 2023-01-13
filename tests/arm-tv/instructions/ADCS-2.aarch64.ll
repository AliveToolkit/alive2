; ModuleID = 'fuzz'
source_filename = "fuzz"
target datalayout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128"
target triple = "aarch64-linux-gnu"

define i32 @f(i1 noundef %0) {
  %2 = zext i1 %0 to i64
  %3 = call i64 @llvm.umul.fix.i64(i64 140737488355328, i64 %2, i32 44)
  %4 = sext i1 %0 to i64
  %5 = call { i64, i1 } @llvm.usub.with.overflow.i64(i64 %4, i64 %3)
  %6 = extractvalue { i64, i1 } %5, 1
  %7 = sext i1 %6 to i64
  %8 = call i64 @llvm.abs.i64(i64 %7, i1 false)
  %9 = zext i1 %6 to i64
  %10 = call { i64, i1 } @llvm.uadd.with.overflow.i64(i64 %8, i64 %9)
  %11 = extractvalue { i64, i1 } %10, 1
  %12 = zext i1 %11 to i32
  ret i32 %12
}

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i64 @llvm.umul.fix.i64(i64, i64, i32 immarg) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare { i64, i1 } @llvm.usub.with.overflow.i64(i64, i64) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i64 @llvm.abs.i64(i64, i1 immarg) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare { i64, i1 } @llvm.uadd.with.overflow.i64(i64, i64) #0

attributes #0 = { nocallback nofree nosync nounwind speculatable willreturn memory(none) }
