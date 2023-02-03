source_filename = "fuzz"
target datalayout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128"
target triple = "aarch64-linux-gnu"

define zeroext i1 @f(i8 %0) {
  %2 = zext i8 %0 to i64
  %3 = select i1 true, i64 %2, i64 -4611686018394882047
  %4 = sext i8 %0 to i64
  %5 = trunc i8 %0 to i1
  %6 = select i1 %5, i64 %4, i64 %3
  %7 = trunc i64 %6 to i32
  %8 = trunc i64 %3 to i32
  %9 = icmp ugt i32 %7, %8
  %10 = trunc i64 %6 to i8
  %11 = sext i1 %9 to i8
  %12 = mul nuw nsw i8 %10, %11
  %13 = trunc i64 %3 to i1
  %14 = trunc i8 %0 to i1
  %15 = trunc i8 %12 to i1
  %16 = select i1 %15, i1 %13, i1 %14
  %17 = trunc i64 %3 to i8
  %18 = call i8 @llvm.ushl.sat.i8(i8 %17, i8 %0)
  %19 = freeze i8 %18
  %20 = zext i8 %19 to i32
  %21 = zext i1 %16 to i32
  %22 = select i1 %9, i32 %20, i32 %21
  %23 = icmp ne i32 -2, %22
  %24 = trunc i32 %22 to i8
  %25 = icmp ult i8 %0, %24
  %26 = zext i1 %23 to i64
  %27 = call i64 @llvm.abs.i64(i64 %26, i1 false)
  %28 = trunc i64 %27 to i1
  %29 = icmp uge i1 %25, %28
  ret i1 %29
}

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i8 @llvm.ushl.sat.i8(i8, i8) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i64 @llvm.smul.fix.i64(i64, i64, i32 immarg) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i32 @llvm.usub.sat.i32(i32, i32) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare { i8, i1 } @llvm.uadd.with.overflow.i8(i8, i8) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i64 @llvm.umax.i64(i64, i64) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i16 @llvm.bswap.i16(i16) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i64 @llvm.abs.i64(i64, i1 immarg) #0

attributes #0 = { nocallback nofree nosync nounwind speculatable willreturn memory(none) }
