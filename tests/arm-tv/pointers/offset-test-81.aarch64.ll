; ModuleID = 'foo.c'
source_filename = "foo.c"
target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128"
target triple = "arm64-apple-macosx13.0.0"

%struct.s = type { i16, i64, i32, i64, i8, i32, i32, i8 }

; Function Attrs: mustprogress nofree norecurse nosync nounwind ssp uwtable willreturn
define void @f(%struct.s* nocapture %0) local_unnamed_addr #0 {
  %2 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 5
  %3 = load i32, i32* %2, align 4
  %4 = sext i32 %3 to i64
  %5 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 3
  store i64 %4, i64* %5, align 8
  %6 = shl i32 %3, 1
  %7 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 6
  %8 = load i32, i32* %7, align 8
  %9 = add nsw i32 %8, %6
  %10 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 0
  %11 = load i16, i16* %10, align 8
  %12 = zext i16 %11 to i32
  %13 = add nsw i32 %9, %12
  %14 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 1
  %15 = load i64, i64* %14, align 8
  %16 = trunc i64 %15 to i32
  %17 = add i32 %13, %16
  %18 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 2
  store i32 %17, i32* %18, align 8
  %19 = trunc i32 %17 to i8
  %20 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 7
  store i8 %19, i8* %20, align 4
  %21 = trunc i32 %17 to i16
  store i16 %21, i16* %10, align 8
  %22 = shl i8 %19, 1
  %23 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 4
  store i8 %22, i8* %23, align 8
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
