; ModuleID = 'foo.c'
source_filename = "foo.c"
target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128"
target triple = "arm64-apple-macosx13.0.0"

%struct.s = type { i32, i32, i8, i16, i16, i32, i16, i32 }

; Function Attrs: mustprogress nofree norecurse nosync nounwind ssp uwtable willreturn
define void @f(%struct.s* nocapture %0) local_unnamed_addr #0 {
  %2 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 5
  %3 = load i32, i32* %2, align 4
  %4 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 1
  store i32 %3, i32* %4, align 4
  %5 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 6
  %6 = load i16, i16* %5, align 4
  %7 = sext i16 %6 to i32
  %8 = add nsw i32 %3, %7
  %9 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 2
  %10 = load i8, i8* %9, align 4
  %11 = sext i8 %10 to i32
  %12 = add nsw i32 %8, %11
  %13 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 7
  %14 = load i32, i32* %13, align 4
  %15 = add nsw i32 %12, %14
  %16 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 3
  %17 = load i16, i16* %16, align 2
  %18 = sext i16 %17 to i32
  %19 = add nsw i32 %15, %18
  store i32 %19, i32* %4, align 4
  %20 = add nsw i32 %19, %18
  %21 = trunc i32 %20 to i16
  store i16 %21, i16* %16, align 2
  %22 = shl i32 %20, 16
  %23 = ashr exact i32 %22, 16
  %24 = add nsw i32 %23, %20
  store i32 %24, i32* %2, align 4
  %25 = trunc i32 %24 to i16
  store i16 %25, i16* %16, align 2
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
