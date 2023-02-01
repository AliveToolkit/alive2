; TEST-ARGS: --disable-undef-input --disable-poison-input --global-isel  -global-isel-abort=0
; ModuleID = 'fuzz'

source_filename = "fuzz"
target datalayout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128"
target triple = "aarch64-linux-gnu"

define i8 @f(i8 %0, i8 %1, i8 noundef signext %2, i8 signext %3, i8 noundef zeroext %4, i8 %5, i8 %6, i8 %7, i8 noundef zeroext %8) {
  %10 = icmp uge i8 %3, -1
  %11 = sext i1 %10 to i8
  %12 = call i8 @llvm.umul.fix.sat.i8(i8 %11, i8 %0, i32 5)
  %13 = and i8 %0, 127
  %14 = icmp slt i8 %8, %3
  %15 = udiv exact i8 %7, %13
  %16 = sext i1 %14 to i8
  %17 = trunc i8 %5 to i1
  %18 = select i1 %17, i8 %16, i8 %12
  %19 = zext i1 %10 to i8
  %20 = trunc i8 %18 to i1
  %21 = select i1 %20, i8 %19, i8 %15
  ret i8 %21
}

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i8 @llvm.umul.fix.sat.i8(i8, i8, i32 immarg) #0

