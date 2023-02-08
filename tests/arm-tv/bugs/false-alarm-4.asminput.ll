target datalayout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128"
target triple = "aarch64-linux-gnu"

define i32 @f(i32 %0) {
  %2 = call { i32, i1 } @llvm.sadd.with.overflow.i32(i32 0, i32 1)
  %3 = extractvalue { i32, i1 } %2, 1
  %4 = icmp sgt i32 -1, %0
  %5 = zext i1 %3 to i32
  %6 = zext i1 %4 to i32
  %7 = or i32 %5, %6
  ret i32 %7
}

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare { i32, i1 } @llvm.sadd.with.overflow.i32(i32, i32) #0

attributes #0 = { nocallback nofree nosync nounwind speculatable willreturn memory(none) }
