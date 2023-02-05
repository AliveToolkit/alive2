; ModuleID = 'foo.c'
source_filename = "foo.c"
target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128"
target triple = "arm64-apple-macosx13.0.0"

%struct.s = type { i64, i16, i32, i8, i64, i64, i8, i32 }

; Function Attrs: mustprogress nofree norecurse nosync nounwind ssp uwtable willreturn
define void @f(%struct.s* nocapture %0) local_unnamed_addr #0 {
  %2 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 6
  %3 = load i8, i8* %2, align 8
  %4 = sext i8 %3 to i32
  %5 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 7
  %6 = load i32, i32* %5, align 4
  %7 = add nsw i32 %6, %4
  %8 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 4
  %9 = load i64, i64* %8, align 8
  %10 = trunc i64 %9 to i32
  %11 = add i32 %7, %10
  %12 = sext i32 %11 to i64
  store i64 %12, i64* %8, align 8
  %13 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 5
  %14 = load i64, i64* %13, align 8
  %15 = trunc i64 %14 to i32
  %16 = add i32 %11, %15
  %17 = sext i32 %16 to i64
  store i64 %17, i64* %13, align 8
  %18 = trunc i32 %16 to i16
  %19 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 1
  store i16 %18, i16* %19, align 8
  %20 = trunc i32 %16 to i8
  store i8 %20, i8* %2, align 8
  %21 = shl i32 %16, 24
  %22 = ashr exact i32 %21, 24
  %23 = add nsw i32 %22, %16
  %24 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 0
  %25 = load i64, i64* %24, align 8
  %26 = trunc i64 %25 to i32
  %27 = add i32 %23, %26
  %28 = sext i32 %27 to i64
  store i64 %28, i64* %24, align 8
  ret void
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
