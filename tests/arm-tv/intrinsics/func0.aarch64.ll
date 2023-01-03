; TEST-ARGS: --disable-undef-input


target datalayout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128"
target triple = "aarch64-unknown-linux-gnu"

define i32 @f(i32 %0) {
  %2 = call i32 @llvm.abs.i32(i32 %0, i1 true)
  ret i32 %2
}

; Function Attrs: nofree nosync nounwind readnone speculatable willreturn
declare i32 @llvm.abs.i32(i32, i1 immarg) #0

attributes #0 = { nofree nosync nounwind readnone speculatable willreturn }
