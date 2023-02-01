; ModuleID = 'fuzz'
source_filename = "fuzz"
target datalayout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128"
target triple = "aarch64-linux-gnu"

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i16 @llvm.smul.fix.i16(i16, i16, i32 immarg) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i54 @llvm.smin.i54(i54, i54) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i16 @llvm.sadd.sat.i16(i16, i16) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i18 @llvm.fshr.i18(i18, i18, i18) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare { i53, i1 } @llvm.sadd.with.overflow.i53(i53, i53) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i18 @llvm.smax.i18(i18, i18) #0

define i16 @f(i64 zeroext %0, i64 zeroext %1, i64 signext %2, i64 %3, i64 signext %4, i64 noundef %5, i64 %6, i64 %7, i64 signext %8, i64 zeroext %9, i64 signext %10, i64 signext %11, i64 %12, i64 noundef %13, i64 zeroext %14) {
  %_t13 = trunc i64 %14 to i58
  %_t12 = trunc i64 %13 to i21
  %_t11 = trunc i64 %12 to i16
  %_t10 = trunc i64 %11 to i60
  %_t9 = trunc i64 %10 to i46
  %_t8 = trunc i64 %9 to i16
  %_t7 = trunc i64 %8 to i23
  %_t6 = trunc i64 %7 to i2
  %_t5 = trunc i64 %6 to i9
  %_t4 = trunc i64 %5 to i16
  %_t3 = trunc i64 %3 to i29
  %_t2 = trunc i64 %2 to i9
  %_t1 = trunc i64 %1 to i16
  %_t = trunc i64 %0 to i58
  %16 = zext i2 %_t6 to i16
  %17 = trunc i64 %4 to i16
  %18 = call i16 @llvm.smul.fix.i16(i16 %16, i16 %17, i32 8)
  %19 = sext i16 %18 to i54
  %20 = freeze i9 %_t5
  %21 = zext i9 %20 to i54
  %22 = call i54 @llvm.smin.i54(i54 %19, i54 %21)
  %23 = trunc i16 %_t11 to i8
  %24 = trunc i23 %_t7 to i8
  %25 = trunc i54 %22 to i1
  %26 = select i1 %25, i8 %23, i8 %24
  %27 = trunc i58 %_t13 to i55
  %28 = sext i16 %_t1 to i55
  %29 = ashr i55 %27, %28
  %30 = sext i8 %26 to i22
  %31 = trunc i55 %29 to i22
  %32 = icmp sgt i22 %30, %31
  %33 = zext i1 %32 to i16
  ret i16 %33
}

attributes #0 = { nocallback nofree nosync nounwind speculatable willreturn memory(none) }
