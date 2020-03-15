; https://bugs.llvm.org/show_bug.cgi?id=27575
source_filename = "27575.src.ll"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

define double @src(i32 %shr) {
entry:
  %cmp = icmp sgt i32 %shr, 0
  %conv = sitofp i32 %shr to double
  %cond = select i1 %cmp, double %conv, double 5.000000e-01
  ret double %cond
}

define double @tgt(i32 %shr) {
entry:
  %0 = icmp sgt i32 %shr, 0
  %1 = select i1 %0, i32 %shr, i32 0
  %2 = sitofp i32 %1 to double
  ret double %2
}

; ERROR: Value mismatch
