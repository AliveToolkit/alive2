; ModuleID = 'foo.c'
source_filename = "foo.c"
target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128"
target triple = "arm64-apple-macosx13.0.0"

%struct.s = type { i32, i16, i16, i32, i8, i32, i16, i64 }

; Function Attrs: mustprogress nofree norecurse nosync nounwind ssp uwtable willreturn
define void @f(%struct.s* nocapture %0) local_unnamed_addr #0 {
  %2 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 2
  store i16 0, i16* %2, align 2
  %3 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 1
  %4 = load i16, i16* %3, align 4
  %5 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 7
  %6 = load i64, i64* %5, align 8
  %7 = zext i16 %4 to i64
  %8 = add i64 %6, %7
  %9 = trunc i64 %8 to i32
  %10 = trunc i64 %8 to i8
  %11 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 4
  store i8 %10, i8* %11, align 4
  %12 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 5
  %13 = load i32, i32* %12, align 8
  %14 = add nsw i32 %13, %9
  %15 = sext i32 %14 to i64
  store i64 %15, i64* %5, align 8
  %16 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 3
  %17 = load i32, i32* %16, align 8
  %18 = add nsw i32 %17, %14
  %19 = sext i32 %18 to i64
  store i64 %19, i64* %5, align 8
  %20 = trunc i32 %18 to i16
  %21 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 6
  store i16 %20, i16* %21, align 4
  %22 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 0
  %23 = load i32, i32* %22, align 8
  %24 = add i32 %18, %13
  %25 = add i32 %24, %23
  %26 = trunc i32 %25 to i8
  store i8 %26, i8* %11, align 4
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
