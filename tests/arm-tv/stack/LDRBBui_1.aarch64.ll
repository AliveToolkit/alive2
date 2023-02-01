; TEST-ARGS: --disable-undef-input --disable-poison-input --global-isel

; ModuleID = 'fuzz'
source_filename = "fuzz"
target datalayout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128"
target triple = "aarch64-linux-gnu"

define signext i16 @f(i22 signext %0, i39 noundef %1, i16 %2, i16 %3, i7 signext %4, i26 noundef %5, i16 noundef zeroext %6, i13 signext %7, i16 %8) {
  %10 = trunc i16 %8 to i8
  %11 = trunc i16 %6 to i8
  %12 = trunc i13 %7 to i1
  %13 = select i1 %12, i8 %10, i8 %11
  %14 = trunc i39 %1 to i16
  %15 = zext i8 %13 to i16
  %16 = icmp ne i16 %14, %15
  %17 = sext i1 %16 to i16
  %18 = freeze i13 %7
  %19 = sext i13 %18 to i16
  %20 = icmp ult i16 %17, %19
  %21 = trunc i22 %0 to i16
  %22 = zext i13 %7 to i16
  %23 = icmp sgt i16 %21, %22
  %24 = sext i1 %23 to i49
  %25 = zext i22 %0 to i49
  %26 = call i49 @llvm.umul.fix.i49(i49 %24, i49 %25, i32 2)
  %27 = trunc i22 %0 to i16
  %28 = trunc i49 %26 to i16
  %29 = mul i16 %27, %28
  %30 = freeze i8 %13
  %31 = trunc i8 %30 to i6
  %32 = zext i1 %20 to i6
  %33 = freeze i16 %29
  %34 = trunc i16 %33 to i1
  %35 = select i1 %34, i6 %31, i6 %32
  %36 = zext i6 %35 to i16
  ret i16 %36
}

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i16 @llvm.sshl.sat.i16(i16, i16) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare { i16, i1 } @llvm.ssub.with.overflow.i16(i16, i16) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i49 @llvm.umul.fix.i49(i49, i49, i32 immarg) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i16 @llvm.cttz.i16(i16, i1 immarg) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i16 @llvm.bitreverse.i16(i16) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i16 @llvm.abs.i16(i16, i1 immarg) #0

attributes #0 = { nocallback nofree nosync nounwind speculatable willreturn memory(none) }
