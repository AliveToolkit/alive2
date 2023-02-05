; ModuleID = 'foo.c'
source_filename = "foo.c"
target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128"
target triple = "arm64-apple-macosx13.0.0"

%struct.s = type { i32, i32, i32, i16, i8, i32, i64, i64 }

; Function Attrs: mustprogress nofree norecurse nosync nounwind ssp uwtable willreturn
define void @f(%struct.s* nocapture %0) local_unnamed_addr #0 {
  %2 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 5
  store i32 0, i32* %2, align 8
  %3 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 3
  store i16 0, i16* %3, align 4
  %4 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 7
  %5 = load i64, i64* %4, align 8
  %6 = trunc i64 %5 to i32
  %7 = shl i64 %5, 32
  %8 = ashr exact i64 %7, 32
  %9 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 6
  store i64 %8, i64* %9, align 8
  %10 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 1
  store i32 %6, i32* %10, align 4
  %11 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 4
  %12 = load i8, i8* %11, align 2
  %13 = sext i8 %12 to i32
  %14 = add nsw i32 %13, %6
  store i32 %14, i32* %2, align 8
  %15 = add i32 %14, %6
  %16 = sext i32 %15 to i64
  store i64 %16, i64* %9, align 8
  %17 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 0
  %18 = load i32, i32* %17, align 8
  %19 = add nsw i32 %15, %18
  %20 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 2
  store i32 %19, i32* %20, align 8
  store i32 %19, i32* %2, align 8
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
