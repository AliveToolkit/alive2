; TEST-ARGS: --disable-undef-input

target datalayout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128"
target triple = "aarch64-unknown-linux-gnu"

define i1 @test19a(i39 %0) {
  %2 = ashr i39 %0, 2

  %3 = icmp eq i39 %2, -1

  ret i1 %3

}
