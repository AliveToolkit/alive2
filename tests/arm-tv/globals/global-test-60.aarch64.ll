; ModuleID = 'foo.c'
source_filename = "foo.c"
target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128"
target triple = "arm64-apple-macosx13.0.0"

%struct.anon = type { i8, i64, i32, i8, i8, i16, i16, i64 }

@s = common local_unnamed_addr global %struct.anon zeroinitializer, align 8

; Function Attrs: mustprogress nofree norecurse nosync nounwind ssp uwtable willreturn
define i32 @f(i32 %0) local_unnamed_addr #0 {
  %2 = sext i32 %0 to i64
  store i64 %2, i64* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 1), align 8
  %3 = trunc i32 %0 to i8
  store i8 %3, i8* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 3), align 4
  store i8 %3, i8* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 4), align 1
  store i64 %2, i64* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 7), align 8
  %4 = shl i32 %0, 24
  %5 = ashr exact i32 %4, 24
  %6 = add nsw i32 %5, %0
  %7 = load i16, i16* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 6), align 8
  %8 = sext i16 %7 to i32
  %9 = add nsw i32 %6, %8
  %10 = sext i32 %9 to i64
  store i64 %10, i64* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 1), align 8
  %11 = trunc i32 %9 to i8
  store i8 %11, i8* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 4), align 1
  %12 = trunc i32 %9 to i16
  store i16 %12, i16* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 5), align 2
  %13 = shl i32 %9, 24
  %14 = ashr exact i32 %13, 24
  %15 = add i32 %9, %0
  %16 = add i32 %15, %14
  %17 = trunc i32 %16 to i8
  store i8 %17, i8* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 3), align 4
  ret i32 %16
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
