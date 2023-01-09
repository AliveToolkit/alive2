; ModuleID = 'fuzz'
source_filename = "fuzz"
target datalayout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128"
target triple = "aarch64-linux-gnu"

define i1 @f(i8 signext %0, i16 noundef signext %1) {
  %3 = freeze i8 %0
  %4 = sext i8 %3 to i16
  %5 = icmp sgt i16 %4, -1371
  %6 = zext i8 %0 to i16
  %7 = icmp slt i16 16, %6
  %8 = sext i1 %7 to i32
  %9 = icmp slt i32 -2, %8
  %10 = icmp slt i1 %5, %9
  ret i1 %10
}

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i32 @llvm.umul.fix.sat.i32(i32, i32, i32 immarg) #0

attributes #0 = { nocallback nofree nosync nounwind speculatable willreturn memory(none) }
