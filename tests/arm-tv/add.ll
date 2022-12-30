; ModuleID = 'add.c'
source_filename = "add.c"
target datalayout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128"
target triple = "aarch64"

; Function Attrs: mustprogress nofree norecurse nosync nounwind readnone uwtable willreturn
define dso_local i32 @add(i32 %a, i32 %b) local_unnamed_addr #0 {
entry:
  %reass.add15 = add i32 %b, %a
  %add2 = shl i32 %reass.add15, 1
  %add3 = add nsw i32 %add2, %a
  %mul = mul nsw i32 %b, %a
  %mul4 = mul nsw i32 %mul, %b
  %sub = sub i32 %add3, %mul4
  %div = sdiv i32 %a, %b
  %add5 = add nsw i32 %sub, %div
  ret i32 %add5
}

attributes #0 = { mustprogress nofree norecurse nosync nounwind readnone uwtable willreturn "frame-pointer"="non-leaf" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="generic" "target-features"="+neon" }

!llvm.module.flags = !{!0, !1, !2, !3, !4, !5, !6}
!llvm.ident = !{!7}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 1, !"branch-target-enforcement", i32 0}
!2 = !{i32 1, !"sign-return-address", i32 0}
!3 = !{i32 1, !"sign-return-address-all", i32 0}
!4 = !{i32 1, !"sign-return-address-with-bkey", i32 0}
!5 = !{i32 7, !"uwtable", i32 1}
!6 = !{i32 7, !"frame-pointer", i32 1}
!7 = !{!"clang version 14.0.0 (https://github.com/llvm/llvm-project.git eec96db184fee4a3e67e9eb97efc29bc7452007c)"}
