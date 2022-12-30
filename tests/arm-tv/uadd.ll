target datalayout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128"

declare i32 @llvm.uadd.sat.i32(i32 %a, i32 %b)

define i32 @foo(i32, i32) {
    %3 = call i32 @llvm.uadd.sat.i32(i32 %0, i32 %1)
    ret i32 %3
}
