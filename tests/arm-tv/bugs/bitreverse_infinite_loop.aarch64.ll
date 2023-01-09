; ModuleID = 'fuzz'
source_filename = "fuzz"
target datalayout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128"
target triple = "aarch64-linux-gnu"

define noundef zeroext i1 @f(i1 zeroext %0, i1 noundef %1) {
  %3 = call i1 @llvm.bitreverse.i1(i1 %1)
  %4 = icmp sgt i1 %3, %0
  ret i1 %4
}

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i1 @llvm.bitreverse.i1(i1) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare { i1, i1 } @llvm.sadd.with.overflow.i1(i1, i1) #0

attributes #0 = { nocallback nofree nosync nounwind speculatable willreturn memory(none) }
