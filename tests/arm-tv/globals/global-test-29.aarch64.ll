; ModuleID = 'foo.c'
source_filename = "foo.c"
target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128"
target triple = "arm64-apple-macosx13.0.0"

%struct.anon = type { i8, i8, i64, i8, i16, i8, i16, i64 }

@s = common local_unnamed_addr global %struct.anon zeroinitializer, align 8

; Function Attrs: mustprogress nofree norecurse nosync nounwind ssp uwtable willreturn
define i32 @f(i32 %0) local_unnamed_addr #0 {
  %2 = load i64, i64* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 7), align 8
  %3 = trunc i64 %2 to i32
  %4 = add i32 %3, %0
  %5 = load i8, i8* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 0), align 8
  %6 = sext i8 %5 to i32
  %7 = add nsw i32 %4, %6
  %8 = trunc i32 %7 to i16
  store i16 %8, i16* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 6), align 2
  %9 = load i8, i8* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 1), align 1
  %10 = zext i8 %9 to i32
  %11 = add nsw i32 %7, %10
  %12 = load i16, i16* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 4), align 2
  %13 = zext i16 %12 to i32
  %14 = add nsw i32 %11, %13
  %15 = shl i32 %7, 16
  %16 = ashr exact i32 %15, 16
  %17 = add nsw i32 %14, %16
  %18 = trunc i32 %17 to i8
  store i8 %18, i8* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 5), align 4
  %19 = trunc i32 %17 to i16
  store i16 %19, i16* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 4), align 2
  store i8 %18, i8* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 5), align 4
  %20 = load i8, i8* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 3), align 8
  %21 = zext i8 %20 to i32
  %22 = add nsw i32 %17, %21
  %23 = sext i32 %22 to i64
  store i64 %23, i64* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 7), align 8
  ret i32 %22
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
