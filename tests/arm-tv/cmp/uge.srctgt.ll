; TEST-ARGS: -backend-tv --disable-undef-input


target datalayout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128"
target triple = "aarch64-unknown-linux-gnu"

define i1 @src(i32 %x, i32 %y) {
  %t = icmp uge i32 %x, %y
  ret i1 %t
}

attributes #0 = { nofree nosync nounwind readnone speculatable willreturn }
