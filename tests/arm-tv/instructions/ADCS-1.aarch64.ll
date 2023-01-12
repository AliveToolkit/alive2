; ModuleID = 'fuzz'
source_filename = "fuzz"
target datalayout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128"
target triple = "aarch64-linux-gnu"

define i64 @f(i1 %0, i32 %1, i64 noundef %2, i16 zeroext %3) {
  %5 = trunc i64 %2 to i16
  %6 = trunc i32 %1 to i16
  %7 = select i1 %0, i16 %5, i16 %6
  %8 = trunc i64 %2 to i32
  %9 = sext i16 %7 to i32
  %10 = call { i32, i1 } @llvm.uadd.with.overflow.i32(i32 %8, i32 %9)
  %11 = extractvalue { i32, i1 } %10, 1
  %12 = zext i1 %11 to i64
  %13 = call { i64, i1 } @llvm.uadd.with.overflow.i64(i64 %2, i64 %12)
  %14 = extractvalue { i64, i1 } %13, 1
  %15 = freeze i1 %14
  %16 = sext i1 %15 to i64
  ret i64 %16
}

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i16 @llvm.fshr.i16(i16, i16, i16) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare { i32, i1 } @llvm.uadd.with.overflow.i32(i32, i32) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare { i64, i1 } @llvm.uadd.with.overflow.i64(i64, i64) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i8 @llvm.fshr.i8(i8, i8, i8) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i1 @llvm.umul.fix.i1(i1, i1, i32 immarg) #0

attributes #0 = { nocallback nofree nosync nounwind speculatable willreturn memory(none) }
