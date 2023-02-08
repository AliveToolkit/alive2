; ModuleID = 'foo.c'
source_filename = "foo.c"
target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128"
target triple = "arm64-apple-macosx13.0.0"

%struct.s = type { i32, i64, i64, i32, i16, i16, i16, i16 }

; Function Attrs: mustprogress nofree norecurse nosync nounwind ssp uwtable willreturn
define void @f(%struct.s* nocapture %0) local_unnamed_addr #0 {
  %2 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 2
  %3 = load i64, i64* %2, align 8
  %4 = trunc i64 %3 to i32
  %5 = shl i64 %3, 32
  %6 = ashr exact i64 %5, 32
  store i64 %6, i64* %2, align 8
  %7 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 5
  %8 = load i16, i16* %7, align 2
  %9 = zext i16 %8 to i32
  %10 = add nsw i32 %9, %4
  %11 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 3
  store i32 %10, i32* %11, align 8
  %12 = shl nsw i32 %10, 1
  %13 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 1
  %14 = load i64, i64* %13, align 8
  %15 = trunc i64 %14 to i32
  %16 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 0
  %17 = load i32, i32* %16, align 8
  %18 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 4
  %19 = load i16, i16* %18, align 4
  %20 = zext i16 %19 to i32
  %21 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 7
  %22 = load i16, i16* %21, align 2
  %23 = sext i16 %22 to i32
  %24 = add i32 %10, %15
  %25 = add i32 %24, %12
  %26 = add i32 %25, %17
  %27 = add i32 %26, %20
  %28 = add i32 %27, %23
  store i32 %28, i32* %11, align 8
  %29 = sext i32 %28 to i64
  store i64 %29, i64* %2, align 8
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
