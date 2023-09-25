; XFAIL: 

; ModuleID = 'fuzz'
source_filename = "fuzz"
target datalayout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128"
target triple = "aarch64-linux-gnu"

define zeroext i9 @f(i12 signext %0) {
  %2 = call i30 @llvm.bitreverse.i30(i30 -46285985)
  %3 = call { i12, i1 } @llvm.smul.with.overflow.i12(i12 %0, i12 -2048)
  %4 = extractvalue { i12, i1 } %3, 1
  %5 = extractvalue { i12, i1 } %3, 0
  %6 = trunc i30 %2 to i12
  %7 = call i12 @llvm.umul.fix.sat.i12(i12 -2040, i12 %6, i32 9)
  %8 = zext i12 %5 to i13
  %9 = trunc i30 %2 to i13
  %10 = add nuw i13 %8, %9
  %11 = trunc i13 %10 to i12
  %12 = lshr i12 %11, %0
  %13 = sext i12 %7 to i63
  %14 = icmp ult i63 %13, 274810273792
  %15 = zext i12 %5 to i18
  %16 = sext i12 %7 to i18
  %17 = trunc i12 %0 to i1
  %18 = select i1 %17, i18 %15, i18 %16
  %19 = sext i1 %14 to i12
  %20 = freeze i12 %12
  %21 = icmp sgt i12 %19, %20
  %22 = zext i1 %4 to i4
  %23 = zext i1 %14 to i4
  %24 = or i4 %22, %23
  %25 = zext i12 %5 to i64
  %26 = zext i12 %0 to i64
  %27 = trunc i4 %24 to i1
  %28 = select i1 %27, i64 %25, i64 %26
  %29 = freeze i64 %28
  %30 = trunc i64 %29 to i59
  %31 = freeze i18 %18
  %32 = sext i18 %31 to i59
  %33 = add nuw i59 %30, %32
  %34 = sext i1 %21 to i12
  %35 = trunc i59 %33 to i12
  %36 = icmp uge i12 %34, %35
  %37 = zext i1 %36 to i9
  ret i9 %37
}

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i30 @llvm.bitreverse.i30(i30) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare { i12, i1 } @llvm.smul.with.overflow.i12(i12, i12) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i12 @llvm.umul.fix.sat.i12(i12, i12, i32 immarg) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i12 @llvm.umul.fix.i12(i12, i12, i32 immarg) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i12 @llvm.ctlz.i12(i12, i1 immarg) #0

attributes #0 = { nocallback nofree nosync nounwind speculatable willreturn memory(none) }
