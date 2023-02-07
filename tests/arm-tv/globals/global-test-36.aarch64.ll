; ModuleID = 'foo.c'
source_filename = "foo.c"
target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128"
target triple = "arm64-apple-macosx13.0.0"

%struct.anon = type { i16, i8, i64, i16, i8, i64, i64, i64 }

@s = common local_unnamed_addr global %struct.anon zeroinitializer, align 8

; Function Attrs: mustprogress nofree norecurse nosync nounwind ssp uwtable willreturn
define signext i16 @f(i16 signext %0) local_unnamed_addr #0 {
  store i16 %0, i16* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 3), align 8
  %2 = load i8, i8* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 4), align 2
  %3 = zext i8 %2 to i16
  %4 = add i16 %3, %0
  %5 = load i64, i64* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 6), align 8
  %6 = trunc i64 %5 to i16
  %7 = add i16 %4, %6
  %8 = sext i16 %7 to i64
  store i64 %8, i64* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 5), align 8
  store i64 %8, i64* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 7), align 8
  store i16 %7, i16* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 3), align 8
  %9 = add i16 %7, %3
  %10 = load i64, i64* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 2), align 8
  %11 = trunc i64 %10 to i16
  %12 = shl i16 %7, 1
  %13 = load i8, i8* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 1), align 2
  %14 = sext i8 %13 to i16
  %15 = add i16 %9, %12
  %16 = add i16 %15, %11
  %17 = add i16 %16, %14
  ret i16 %17
}

attributes #0 = { mustprogress nofree norecurse nosync nounwind ssp uwtable willreturn "frame-pointer"="non-leaf" "min-legal-vector-width"="0" "no-trapping-math"="true" "probe-stack"="__chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="apple-m1" "target-features"="+aes,+crc,+crypto,+dotprod,+fp-armv8,+fp16fml,+fullfp16,+lse,+neon,+ras,+rcpc,+rdm,+sha2,+sha3,+sm4,+v8.5a,+zcm,+zcz" }

!llvm.module.flags = !{!0, !1, !2, !3, !4, !5, !6, !7, !8}
!llvm.ident = !{!9}

!0 = !{i32 2, !"SDK Version", [2 x i32] [i32 13, i32 1]}
!1 = !{i32 1, !"wchar_size", i32 4}
!2 = !{i32 1, !"branch-target-enforcement", i32 0}
!3 = !{i32 1, !"sign-return-address", i32 0}
!4 = !{i32 1, !"sign-return-address-all", i32 0}
!5 = !{i32 1, !"sign-return-address-with-bkey", i32 0}
!6 = !{i32 7, !"PIC Level", i32 2}
!7 = !{i32 7, !"uwtable", i32 1}
!8 = !{i32 7, !"frame-pointer", i32 1}
!9 = !{!"Apple clang version 14.0.0 (clang-1400.0.29.202)"}
