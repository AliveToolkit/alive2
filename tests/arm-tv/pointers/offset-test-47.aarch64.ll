; ModuleID = 'foo.c'
source_filename = "foo.c"
target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128"
target triple = "arm64-apple-macosx13.0.0"

%struct.s = type { i64, i64, i16, i8, i64, i8, i8, i32 }

; Function Attrs: mustprogress nofree norecurse nosync nounwind ssp uwtable willreturn
define void @f(%struct.s* nocapture %0) local_unnamed_addr #0 {
  %2 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 1
  %3 = load i64, i64* %2, align 8
  %4 = shl i64 %3, 32
  %5 = ashr exact i64 %4, 32
  store i64 %5, i64* %2, align 8
  %6 = trunc i64 %3 to i8
  %7 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 3
  store i8 %6, i8* %7, align 2
  %8 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 6
  store i8 %6, i8* %8, align 1
  store i8 %6, i8* %7, align 2
  %9 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 4
  store i64 %5, i64* %9, align 8
  store i8 %6, i8* %8, align 1
  %10 = trunc i64 %3 to i16
  %11 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 2
  store i16 %10, i16* %11, align 8
  store i8 %6, i8* %8, align 1
  store i8 %6, i8* %7, align 2
  %12 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 0
  store i64 %5, i64* %12, align 8
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
