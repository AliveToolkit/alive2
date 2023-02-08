; ModuleID = 'foo.c'
source_filename = "foo.c"
target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128"
target triple = "arm64-apple-macosx13.0.0"

%struct.s = type { i32, i64, i64, i64, i64, i8, i32, i16 }

; Function Attrs: mustprogress nofree norecurse nosync nounwind ssp uwtable willreturn
define void @f(%struct.s* nocapture %0) local_unnamed_addr #0 {
  %2 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 5
  %3 = load i8, i8* %2, align 8
  %4 = zext i8 %3 to i32
  %5 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 0
  %6 = load i32, i32* %5, align 8
  %7 = add nsw i32 %6, %4
  %8 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 2
  %9 = load i64, i64* %8, align 8
  %10 = trunc i64 %9 to i32
  %11 = add i32 %7, %10
  %12 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 6
  %13 = load i32, i32* %12, align 4
  %14 = add i32 %11, %13
  %15 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 7
  %16 = load i16, i16* %15, align 8
  %17 = sext i16 %16 to i32
  %18 = add nsw i32 %14, %17
  store i32 %18, i32* %5, align 8
  %19 = shl nsw i32 %18, 1
  %20 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 1
  %21 = load i64, i64* %20, align 8
  %22 = trunc i64 %21 to i32
  %23 = add i32 %19, %22
  %24 = trunc i32 %23 to i8
  store i8 %24, i8* %2, align 8
  %25 = sext i32 %23 to i64
  store i64 %25, i64* %8, align 8
  store i8 %24, i8* %2, align 8
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
