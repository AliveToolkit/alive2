; ModuleID = 'foo.c'
source_filename = "foo.c"
target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128"
target triple = "arm64-apple-macosx13.0.0"

%struct.s = type { i8, i64, i16, i64, i8, i64, i64, i32 }

; Function Attrs: mustprogress nofree norecurse nosync nounwind ssp uwtable willreturn
define void @f(%struct.s* nocapture %0) local_unnamed_addr #0 {
  %2 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 3
  %3 = load i64, i64* %2, align 8
  %4 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 6
  %5 = load i64, i64* %4, align 8
  %6 = add i64 %5, %3
  %7 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 5
  %8 = load i64, i64* %7, align 8
  %9 = add i64 %6, %8
  %10 = trunc i64 %9 to i32
  %11 = trunc i64 %9 to i16
  %12 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 2
  store i16 %11, i16* %12, align 8
  %13 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 4
  %14 = load i8, i8* %13, align 8
  %15 = zext i8 %14 to i32
  %16 = add nsw i32 %15, %10
  %17 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 0
  %18 = load i8, i8* %17, align 8
  %19 = sext i8 %18 to i32
  %20 = add nsw i32 %16, %19
  %21 = trunc i32 %20 to i8
  store i8 %21, i8* %13, align 8
  %22 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 1
  %23 = load i64, i64* %22, align 8
  %24 = trunc i64 %23 to i32
  %25 = add i32 %20, %24
  %26 = trunc i32 %25 to i8
  store i8 %26, i8* %13, align 8
  %27 = trunc i64 %3 to i32
  %28 = add i32 %25, %27
  %29 = and i32 %25, 255
  %30 = add nsw i32 %28, %29
  %31 = sext i32 %30 to i64
  store i64 %31, i64* %2, align 8
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
