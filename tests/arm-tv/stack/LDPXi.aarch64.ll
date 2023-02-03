; TEST-ARGS: --global-isel -global-isel-abort=0

; ModuleID = 'fuzz'
source_filename = "fuzz"
target datalayout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128"
target triple = "aarch64-linux-gnu"

define i64 @f(i1 %0, i8 %1, i8 zeroext %2, i8 noundef %3, i64 %4, i8 %5, i8 signext %6, i8 signext %7, i64 %8, i8 noundef signext %9, i8 %10, i1 zeroext %11, i1 zeroext %12) {
  %14 = sext i8 %9 to i16
  %15 = sdiv exact i16 %14, -2
  %16 = trunc i16 %15 to i8
  %17 = icmp sge i8 %7, %16
  %18 = sext i1 %17 to i64
  %19 = freeze i8 %7
  %20 = trunc i8 %19 to i1
  %21 = select i1 %20, i64 %4, i64 %18
  %22 = sext i8 %10 to i32
  %23 = freeze i64 %21
  %24 = trunc i64 %23 to i32
  %25 = trunc i8 %7 to i1
  %26 = select i1 %25, i32 %22, i32 %24
  %27 = sext i32 %26 to i64
  ret i64 %27
}

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare { i64, i1 } @llvm.uadd.with.overflow.i64(i64, i64) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i64 @llvm.smul.fix.i64(i64, i64, i32 immarg) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i8 @llvm.sadd.sat.i8(i8, i8) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i16 @llvm.cttz.i16(i16, i1 immarg) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i16 @llvm.bitreverse.i16(i16) #0

attributes #0 = { nocallback nofree nosync nounwind speculatable willreturn memory(none) }
