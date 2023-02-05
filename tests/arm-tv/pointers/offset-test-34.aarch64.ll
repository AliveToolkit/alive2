; ModuleID = 'foo.c'
source_filename = "foo.c"
target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128"
target triple = "arm64-apple-macosx13.0.0"

%struct.s = type { i64, i8, i64, i16, i8, i8, i32, i16 }

; Function Attrs: mustprogress nofree norecurse nosync nounwind ssp uwtable willreturn
define void @f(%struct.s* nocapture %0) local_unnamed_addr #0 {
  %2 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 5
  %3 = load i8, i8* %2, align 1
  %4 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 2
  %5 = load i64, i64* %4, align 8
  %6 = zext i8 %3 to i64
  %7 = add nsw i64 %5, %6
  %8 = trunc i64 %7 to i32
  %9 = trunc i64 %7 to i16
  %10 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 7
  store i16 %9, i16* %10, align 8
  %11 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 6
  store i32 %8, i32* %11, align 4
  %12 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 3
  %13 = load i16, i16* %12, align 8
  %14 = zext i16 %13 to i32
  %15 = shl nuw nsw i32 %14, 1
  %16 = zext i8 %3 to i32
  %17 = add i32 %8, %16
  %18 = add i32 %17, %14
  %19 = add i32 %18, %15
  %20 = trunc i32 %19 to i8
  store i8 %20, i8* %2, align 1
  %21 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 1
  %22 = load i8, i8* %21, align 8
  %23 = sext i8 %22 to i32
  %24 = add i32 %23, %8
  %25 = add i32 %24, %19
  %26 = sext i32 %25 to i64
  %27 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 0
  store i64 %26, i64* %27, align 8
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
