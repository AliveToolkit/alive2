; TEST-ARGS: -backend-tv --disable-undef-input


target datalayout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128"
target triple = "aarch64-unknown-linux-gnu"

define i32 @f(i32 %0, i32 %1) {
  %3 = call i32 @llvm.fshr.i32(i32 8, i32 %0, i32 %1)
  ret i32 %3
}

; Function Attrs: nofree nosync nounwind readnone speculatable willreturn
declare i32 @llvm.fshr.i32(i32, i32, i32) #0

attributes #0 = { nofree nosync nounwind readnone speculatable willreturn }
