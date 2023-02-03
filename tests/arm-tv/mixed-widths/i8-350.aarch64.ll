; TEST-ARGS:

target datalayout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128"
target triple = "aarch64-unknown-linux-gnu"

define i8 @f(i8 %0) {
  %2 = call { i8, i1 } @llvm.ssub.with.overflow.i8(i8 -3, i8 %0)
  %3 = extractvalue { i8, i1 } %2, 0
  %4 = extractvalue { i8, i1 } %2, 1
  ret i8 %3
}

; Function Attrs: nofree nosync nounwind readnone speculatable willreturn
declare { i8, i1 } @llvm.ssub.with.overflow.i8(i8, i8) #0

attributes #0 = { nofree nosync nounwind readnone speculatable willreturn }
