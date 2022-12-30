; TEST-ARGS: -backend-tv --disable-undef-input

target datalayout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128"
target triple = "aarch64-unknown-linux-gnu"

define i64 @f(i64 %0, i64 %1) {
  %3 = call i64 @llvm.uadd.sat.i64(i64 %0, i64 %1)
  ret i64 %3
}

; Function Attrs: nofree nosync nounwind readnone speculatable willreturn
declare i64 @llvm.uadd.sat.i64(i64, i64) #0

attributes #0 = { nofree nosync nounwind readnone speculatable willreturn }
