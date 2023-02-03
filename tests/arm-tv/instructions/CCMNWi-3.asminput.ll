; ModuleID = 'fuzz'
source_filename = "fuzz"
target datalayout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128"
target triple = "aarch64-linux-gnu"

define zeroext i32 @f(i32 zeroext %0, i32 %1) {
  %3 = or i32 %0, -1
  %4 = call i32 @llvm.bswap.i32(i32 %3)
  %5 = freeze i32 %4
  %6 = call i32 @llvm.usub.sat.i32(i32 %5, i32 %1)
  %7 = freeze i32 %4
  %8 = srem i32 %7, %0
  %9 = icmp sgt i32 %6, 0
  %10 = call i32 @llvm.smul.fix.i32(i32 %6, i32 %1, i32 16)
  %11 = trunc i32 %8 to i1
  %12 = select i1 %11, i32 %0, i32 -17
  %13 = zext i1 %9 to i32
  %14 = add nuw i32 %10, %13
  %15 = icmp slt i32 %12, %14
  %16 = call { i32, i1 } @llvm.usub.with.overflow.i32(i32 %8, i32 %4)
  %17 = extractvalue { i32, i1 } %16, 1
  %18 = sext i1 %15 to i32
  %19 = zext i1 %17 to i32
  %20 = and i32 %18, %19
  ret i32 %20
}

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i32 @llvm.bswap.i32(i32) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i32 @llvm.usub.sat.i32(i32, i32) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i32 @llvm.smul.fix.i32(i32, i32, i32 immarg) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare { i32, i1 } @llvm.usub.with.overflow.i32(i32, i32) #0

attributes #0 = { nocallback nofree nosync nounwind speculatable willreturn memory(none) }
