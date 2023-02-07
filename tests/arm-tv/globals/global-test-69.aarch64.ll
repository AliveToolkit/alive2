; ModuleID = 'foo.c'
source_filename = "foo.c"
target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128"
target triple = "arm64-apple-macosx13.0.0"

%struct.anon = type { i8, i8, i16, i8, i64, i64, i64, i8 }

@s = common local_unnamed_addr global %struct.anon zeroinitializer, align 8

; Function Attrs: mustprogress nofree norecurse nosync nounwind ssp uwtable willreturn
define i32 @f(i32 %0) local_unnamed_addr #0 {
  %2 = load i8, i8* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 3), align 4
  %3 = zext i8 %2 to i32
  %4 = add i32 %3, %0
  %5 = load i64, i64* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 5), align 8
  %6 = trunc i64 %5 to i32
  %7 = add i32 %4, %6
  %8 = trunc i32 %7 to i8
  store i8 %8, i8* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 3), align 4
  %9 = load i8, i8* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 0), align 8
  %10 = zext i8 %9 to i32
  %11 = add i32 %7, %10
  %12 = load i64, i64* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 6), align 8
  %13 = trunc i64 %12 to i32
  %14 = add i32 %11, %13
  %15 = trunc i32 %14 to i8
  store i8 %15, i8* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 7), align 8
  %16 = shl i32 %14, 24
  %17 = ashr exact i32 %16, 24
  %18 = load i64, i64* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 4), align 8
  %19 = trunc i64 %18 to i32
  %20 = and i32 %7, 255
  %21 = add i32 %14, %20
  %22 = add i32 %21, %19
  %23 = add i32 %22, %17
  %24 = trunc i32 %23 to i8
  store i8 %24, i8* getelementptr inbounds (%struct.anon, %struct.anon* @s, i64 0, i32 1), align 1
  %25 = add i32 %10, %6
  %26 = add i32 %25, %23
  ret i32 %26
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
