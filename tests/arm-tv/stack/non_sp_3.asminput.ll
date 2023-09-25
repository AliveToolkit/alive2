; XFAIL: 

; ModuleID = 'fuzz'
source_filename = "fuzz"
target datalayout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128"
target triple = "aarch64-linux-gnu"

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i32 @llvm.smul.fix.sat.i32(i32, i32, i32 immarg) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i32 @llvm.bswap.i32(i32) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i32 @llvm.umul.fix.sat.i32(i32, i32, i32 immarg) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i32 @llvm.ssub.sat.i32(i32, i32) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i32 @llvm.sadd.sat.i32(i32, i32) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare { i32, i1 } @llvm.uadd.with.overflow.i32(i32, i32) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i32 @llvm.fshl.i32(i32, i32, i32) #0

define noundef i32 @f(i64 %0, i64 zeroext %1, i64 signext %2, i64 signext %3, i64 signext %4, i64 signext %5, i64 %6, i64 zeroext %7, i64 %8, i64 noundef zeroext %9, i64 noundef signext %10, i64 %11, i64 signext %12) {
  %_t12 = trunc i64 %12 to i32
  %_t11 = trunc i64 %11 to i32
  %_t10 = trunc i64 %10 to i32
  %_t9 = trunc i64 %9 to i32
  %_t8 = trunc i64 %8 to i32
  %_t7 = trunc i64 %7 to i32
  %_t6 = trunc i64 %6 to i32
  %_t5 = trunc i64 %5 to i32
  %_t4 = trunc i64 %4 to i32
  %_t3 = trunc i64 %3 to i32
  %_t2 = trunc i64 %2 to i32
  %_t1 = trunc i64 %1 to i32
  %_t = trunc i64 %0 to i32
  %14 = trunc i32 %_t2 to i1
  %15 = select i1 %14, i32 %_t11, i32 %_t12
  %16 = icmp ne i32 %_t10, %15
  %17 = call i32 @llvm.umul.fix.sat.i32(i32 %_t1, i32 %_t9, i32 26)
  %18 = sext i1 %16 to i32
  %19 = trunc i32 %_t2 to i1
  %20 = select i1 %19, i32 %17, i32 %18
  ret i32 %20
}

attributes #0 = { nocallback nofree nosync nounwind speculatable willreturn memory(none) }

