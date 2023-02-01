; ModuleID = 'fuzz'
source_filename = "fuzz"
target datalayout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128"
target triple = "aarch64-linux-gnu"

define signext i32 @f(i16 noundef %0, i16 noundef %1, i1 zeroext %2, i1 noundef zeroext %3, i64 %4, i16 %5, i64 noundef %6, i8 noundef signext %7, i32 %8, i32 signext %9, i8 noundef %10, i64 zeroext %11, i1 zeroext %12, i64 %13, i16 noundef signext %14, i64 signext %15) {
  %17 = trunc i64 %11 to i8
  %18 = trunc i64 %15 to i1
  %19 = select i1 %18, i8 %10, i8 %17
  %20 = trunc i64 %4 to i1
  %21 = call i1 @llvm.umul.fix.i1(i1 %12, i1 %20, i32 0)
  %22 = trunc i64 %4 to i8
  %23 = sext i1 %21 to i8
  %24 = icmp sle i8 %22, %23
  %25 = sext i8 %19 to i16
  %26 = zext i1 %24 to i16
  %27 = icmp sgt i16 %25, %26
  %28 = freeze i1 %27
  %29 = sext i1 %28 to i64
  %30 = urem i64 %29, %13
  %31 = trunc i64 %30 to i32
  ret i32 %31
}

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i1 @llvm.umul.fix.i1(i1, i1, i32 immarg) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i16 @llvm.umin.i16(i16, i16) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i1 @llvm.fshl.i1(i1, i1, i1) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i8 @llvm.ctlz.i8(i8, i1 immarg) #0

attributes #0 = { nocallback nofree nosync nounwind speculatable willreturn memory(none) }
