; ModuleID = 'foo.c'
source_filename = "foo.c"
target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128"
target triple = "arm64-apple-macosx13.0.0"

%struct.anon = type { i8, i8, i16, i64, i32, i32, i8, i16 }

@s = common local_unnamed_addr global %struct.anon zeroinitializer, align 8

; Function Attrs: mustprogress nofree norecurse nosync nounwind ssp uwtable willreturn
define signext i8 @f(i8 signext %0) local_unnamed_addr #0 {
  %2 = load i8, i8* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 0), align 8
  %3 = add i8 %2, %0
  %4 = load i32, i32* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 5), align 4
  %5 = trunc i32 %4 to i8
  %6 = add i8 %3, %5
  %7 = load i8, i8* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 6), align 8
  %8 = add i8 %6, %7
  %9 = load i16, i16* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 2), align 2
  %10 = trunc i16 %9 to i8
  %11 = add i8 %8, %10
  store i8 %11, i8* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 0), align 8
  %12 = shl i8 %11, 1
  %13 = load i8, i8* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 1), align 1
  %14 = add i8 %12, %13
  store i8 %14, i8* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 0), align 8
  %15 = load i64, i64* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 3), align 8
  %16 = trunc i64 %15 to i8
  %17 = add i8 %14, %16
  %18 = sext i8 %17 to i16
  store i16 %18, i16* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 2), align 2
  %19 = sext i8 %17 to i32
  store i32 %19, i32* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 4), align 8
  store i8 %17, i8* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 6), align 8
  ret i8 %17
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
