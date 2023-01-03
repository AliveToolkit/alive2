; TEST-ARGS: --disable-undef-input

target datalayout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128"
target triple = "aarch64-unknown-linux-gnu"

define i16 @f(i16 %0) {
  %2 = call i16 @llvm.uadd.sat.i16(i16 %0, i16 32767)
  ret i16 %2
}

; Function Attrs: nofree nosync nounwind readnone speculatable willreturn
declare i16 @llvm.uadd.sat.i16(i16, i16) #0

attributes #0 = { nofree nosync nounwind readnone speculatable willreturn }
