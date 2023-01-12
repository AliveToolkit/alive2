; ModuleID = 'fuzz'
source_filename = "fuzz"
target datalayout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128"
target triple = "aarch64-linux-gnu"

define zeroext i20 @f(i28 signext %0, i62 noundef %1, i58 noundef signext %2, i41 zeroext %3) {
  %5 = sext i28 %0 to i64
  %6 = sext i58 %2 to i64
  %7 = call { i64, i1 } @llvm.uadd.with.overflow.i64(i64 %5, i64 %6)
  %8 = extractvalue { i64, i1 } %7, 1
  %9 = trunc i58 %2 to i57
  %10 = sext i1 %8 to i57
  %11 = sub nuw i57 %9, %10
  %12 = sext i28 %0 to i50
  %13 = trunc i57 %11 to i50
  %14 = add nuw nsw i50 %12, %13
  %15 = trunc i50 %14 to i20
  ret i20 %15
}

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare { i64, i1 } @llvm.uadd.with.overflow.i64(i64, i64) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i32 @llvm.bitreverse.i32(i32) #0

attributes #0 = { nocallback nofree nosync nounwind speculatable willreturn memory(none) }
