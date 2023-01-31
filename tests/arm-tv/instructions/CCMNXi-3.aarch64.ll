; ModuleID = 'fuzz'
source_filename = "fuzz"
target datalayout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128"
target triple = "aarch64-linux-gnu"

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare { i41, i1 } @llvm.smul.with.overflow.i41(i41, i41) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i41 @llvm.ctlz.i41(i41, i1 immarg) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i41 @llvm.fshr.i41(i41, i41, i41) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare { i41, i1 } @llvm.sadd.with.overflow.i41(i41, i41) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i41 @llvm.umul.fix.sat.i41(i41, i41, i32 immarg) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i41 @llvm.smax.i41(i41, i41) #0

define i41 @f(i64 zeroext %0) {
  %_t = trunc i64 %0 to i41
  %2 = call { i41, i1 } @llvm.smul.with.overflow.i41(i41 1086593171964, i41 -1086593171965)
  %3 = extractvalue { i41, i1 } %2, 1
  %4 = extractvalue { i41, i1 } %2, 0
  %5 = zext i1 %3 to i41
  %6 = select i1 false, i41 1086593171960, i41 %5
  %7 = zext i1 %3 to i41
  %8 = call i41 @llvm.ctlz.i41(i41 %7, i1 false)
  %9 = urem i41 1086593171966, %6
  %10 = udiv i41 %8, %6
  %11 = call i41 @llvm.fshr.i41(i41 -1086593171965, i41 %10, i41 %6)
  %12 = icmp uge i41 %8, %9
  %13 = sext i1 %3 to i41
  %14 = select i1 true, i41 %13, i41 %11
  %15 = zext i1 %12 to i41
  %16 = zext i1 %12 to i41
  %17 = icmp ule i41 %15, %16
  %18 = sext i1 %17 to i41
  %mask3 = and i41 -20827, 63
  %19 = ashr i41 %18, %mask3
  %20 = shl nuw i41 %19, %8
  %21 = trunc i41 %14 to i1
  %22 = select i1 %21, i41 %20, i41 %4
  ret i41 %22
}

attributes #0 = { nocallback nofree nosync nounwind speculatable willreturn memory(none) }
