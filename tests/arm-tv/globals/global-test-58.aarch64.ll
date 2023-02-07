; ModuleID = 'foo.c'
source_filename = "foo.c"
target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128"
target triple = "arm64-apple-macosx13.0.0"

%struct.anon = type { i8, i16, i8, i16, i16, i64, i32, i16 }

@s = common local_unnamed_addr global %struct.anon zeroinitializer, align 8

; Function Attrs: mustprogress nofree norecurse nosync nounwind ssp uwtable willreturn
define i32 @f(i32 %0) local_unnamed_addr #0 {
  %2 = zext i32 %0 to i64
  store i64 %2, i64* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 5), align 8
  %3 = trunc i32 %0 to i8
  store i8 %3, i8* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 0), align 8
  %4 = trunc i32 %0 to i16
  store i16 %4, i16* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 7), align 4
  store i64 %2, i64* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 5), align 8
  store i16 %4, i16* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 7), align 4
  %5 = load i16, i16* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 3), align 2
  %6 = zext i16 %5 to i32
  %7 = add i32 %6, %0
  %8 = trunc i32 %7 to i16
  store i16 %8, i16* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 7), align 4
  %9 = zext i32 %7 to i64
  store i64 %9, i64* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 5), align 8
  store i16 %8, i16* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 7), align 4
  %10 = shl i32 %0, 24
  %11 = ashr exact i32 %10, 24
  %12 = add i32 %7, %11
  %13 = load i16, i16* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 1), align 2
  %14 = zext i16 %13 to i32
  %15 = add i32 %12, %14
  ret i32 %15
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
