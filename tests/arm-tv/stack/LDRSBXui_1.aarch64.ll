; TEST-ARGS: --disable-undef-input --disable-poison-input --global-isel  -global-isel-abort=0
; ModuleID = 'fuzz'

; ModuleID = 'fuzz'
source_filename = "fuzz"
target datalayout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128"
target triple = "aarch64-linux-gnu"

define zeroext i31 @f(i1 signext %0, i25 %1, i40 zeroext %2, i16 %3, i25 %4, i12 signext %5, i57 noundef zeroext %6, i45 %7, i8 signext %8, i37 %9) {
  %11 = sext i12 %5 to i30
  %12 = sext i12 %5 to i30
  %13 = and i30 %11, %12
  %14 = sext i8 %8 to i64
  %15 = icmp sle i64 2681, %14
  %16 = sext i1 %15 to i11
  %17 = trunc i12 %5 to i11
  %18 = call i11 @llvm.sshl.sat.i11(i11 %16, i11 %17)
  %19 = trunc i40 %2 to i25
  %20 = icmp slt i25 %19, %1
  %21 = sext i25 %1 to i57
  %22 = zext i30 %13 to i57
  %23 = mul nuw nsw i57 %21, %22
  %24 = sext i57 %23 to i61
  %25 = sext i1 %20 to i61
  %26 = trunc i11 %18 to i1
  %27 = select i1 %26, i61 %24, i61 %25
  %28 = trunc i61 %27 to i5
  %29 = trunc i16 %3 to i5
  %30 = trunc i57 %23 to i1
  %31 = select i1 %30, i5 %28, i5 %29
  %32 = zext i5 %31 to i31
  ret i31 %32
}

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i11 @llvm.sshl.sat.i11(i11, i11) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i59 @llvm.cttz.i59(i59, i1 immarg) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i48 @llvm.umul.fix.i48(i48, i48, i32 immarg) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i32 @llvm.bitreverse.i32(i32) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i31 @llvm.bitreverse.i31(i31) #0

attributes #0 = { nocallback nofree nosync nounwind speculatable willreturn memory(none) }
