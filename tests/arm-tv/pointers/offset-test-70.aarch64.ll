; ModuleID = 'foo.c'
source_filename = "foo.c"
target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128"
target triple = "arm64-apple-macosx13.0.0"

%struct.s = type { i64, i16, i32, i64, i8, i8, i16, i64 }

; Function Attrs: mustprogress nofree norecurse nosync nounwind ssp uwtable willreturn
define void @f(%struct.s* nocapture %0) local_unnamed_addr #0 {
  %2 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 7
  %3 = load i64, i64* %2, align 8
  %4 = trunc i64 %3 to i16
  %5 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 6
  store i16 %4, i16* %5, align 2
  %6 = shl i64 %3, 32
  %7 = ashr exact i64 %6, 32
  %8 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 3
  store i64 %7, i64* %8, align 8
  %9 = trunc i64 %3 to i8
  %10 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 5
  store i8 %9, i8* %10, align 1
  %11 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 0
  store i64 %7, i64* %11, align 8
  %12 = trunc i64 %3 to i32
  %13 = shl i32 %12, 1
  %14 = sext i16 %4 to i32
  %15 = add nsw i32 %13, %14
  %16 = sext i32 %15 to i64
  store i64 %16, i64* %2, align 8
  %17 = trunc i32 %15 to i8
  %18 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 4
  store i8 %17, i8* %18, align 8
  %19 = trunc i32 %15 to i16
  %20 = add i16 %19, %4
  %21 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 1
  store i16 %20, i16* %21, align 8
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
