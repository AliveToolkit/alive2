; ModuleID = 'foo.c'
source_filename = "foo.c"
target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128"
target triple = "arm64-apple-macosx13.0.0"

%struct.anon = type { i32, i32, i16, i8, i32, i8, i32, i8 }

@s = common local_unnamed_addr global %struct.anon zeroinitializer, align 4

; Function Attrs: mustprogress nofree norecurse nosync nounwind ssp uwtable willreturn
define i64 @f(i64 %0) local_unnamed_addr #0 {
  %2 = trunc i64 %0 to i8
  store i8 %2, i8* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 3), align 2
  %3 = load i32, i32* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 0), align 4
  %4 = sext i32 %3 to i64
  %5 = add nsw i64 %4, %0
  %6 = trunc i64 %5 to i32
  store i32 %6, i32* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 6), align 4
  %7 = trunc i64 %5 to i16
  store i16 %7, i16* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 2), align 4
  %8 = shl i64 %5, 32
  %9 = ashr exact i64 %8, 32
  %10 = add nsw i64 %9, %5
  %11 = trunc i64 %10 to i8
  store i8 %11, i8* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 5), align 4
  %12 = trunc i64 %10 to i32
  store i32 %12, i32* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 1), align 4
  %13 = and i64 %0, 255
  %14 = add nsw i64 %10, %13
  %15 = trunc i64 %14 to i32
  store i32 %15, i32* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 0), align 4
  %16 = trunc i64 %14 to i8
  store i8 %16, i8* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 5), align 4
  ret i64 %14
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
