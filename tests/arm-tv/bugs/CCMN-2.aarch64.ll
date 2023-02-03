; ModuleID = 'fuzz'
source_filename = "fuzz"
target datalayout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128"
target triple = "aarch64-linux-gnu"

define i64 @f(i32 signext %0) {
  %2 = select i1 false, i8 127, i8 -125
  %3 = zext i8 %2 to i16
  %4 = freeze i32 %0
  %5 = trunc i32 %4 to i16
  %6 = icmp uge i16 %3, %5
  %7 = zext i32 %0 to i64
  %8 = icmp ugt i64 18014399851659264, %7
  %9 = sext i1 %6 to i8
  %10 = sext i1 %8 to i8
  %11 = icmp slt i8 %9, %10
  %12 = zext i1 %8 to i64
  %13 = sext i1 %8 to i64
  %14 = icmp sgt i64 %12, %13
  %15 = select i1 %6, i1 %11, i1 %14
  %16 = sext i1 %15 to i64
  ret i64 %16
}

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i64 @llvm.ctlz.i64(i64, i1 immarg) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i32 @llvm.sadd.sat.i32(i32, i32) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i8 @llvm.smin.i8(i8, i8) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i32 @llvm.umax.i32(i32, i32) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i8 @llvm.fshr.i8(i8, i8, i8) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i1 @llvm.smul.fix.i1(i1, i1, i32 immarg) #0

attributes #0 = { nocallback nofree nosync nounwind speculatable willreturn memory(none) }
