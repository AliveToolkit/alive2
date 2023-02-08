; ModuleID = 'foo.c'
source_filename = "foo.c"
target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128"
target triple = "arm64-apple-macosx13.0.0"

%struct.anon = type { i8, i32, i64, i32, i8, i16, i8, i8 }

@s = common local_unnamed_addr global %struct.anon zeroinitializer, align 8

; Function Attrs: mustprogress nofree norecurse nosync nounwind ssp uwtable willreturn
define zeroext i16 @f(i16 zeroext %0) local_unnamed_addr #0 {
  %2 = load i64, i64* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 2), align 8
  %3 = trunc i64 %2 to i16
  %4 = add i16 %3, %0
  %5 = trunc i16 %4 to i8
  store i8 %5, i8* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 4), align 4
  %6 = load i8, i8* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 7), align 1
  %7 = zext i8 %6 to i16
  %8 = load i8, i8* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 0), align 8
  %9 = zext i8 %8 to i16
  %10 = add nuw nsw i16 %9, %7
  %11 = shl nuw nsw i16 %10, 1
  %12 = add i16 %4, %3
  %13 = add i16 %12, %11
  %14 = trunc i16 %13 to i8
  store i8 %14, i8* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 4), align 4
  %15 = load i32, i32* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 1), align 4
  %16 = trunc i32 %15 to i16
  %17 = add i16 %13, %16
  %18 = zext i16 %17 to i32
  store i32 %18, i32* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 3), align 8
  %19 = load i16, i16* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 5), align 2
  %20 = add i16 %17, %19
  ret i16 %20
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
