; ModuleID = 'foo.c'
source_filename = "foo.c"
target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128"
target triple = "arm64-apple-macosx13.0.0"

%struct.s = type { i32, i8, i32, i8, i64, i16, i32, i64 }

; Function Attrs: mustprogress nofree norecurse nosync nounwind ssp uwtable willreturn
define void @f(%struct.s* nocapture %0) local_unnamed_addr #0 {
  %2 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 7
  store i64 0, i64* %2, align 8
  %3 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 6
  store i32 0, i32* %3, align 4
  %4 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 2
  %5 = load i32, i32* %4, align 8
  %6 = trunc i32 %5 to i8
  %7 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 3
  store i8 %6, i8* %7, align 4
  %8 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 0
  %9 = load i32, i32* %8, align 8
  %10 = add nsw i32 %9, %5
  %11 = trunc i32 %10 to i16
  %12 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 5
  store i16 %11, i16* %12, align 8
  %13 = shl i32 %10, 16
  %14 = ashr exact i32 %13, 15
  %15 = and i32 %5, 255
  %16 = add i32 %15, %5
  %17 = add i32 %16, %10
  %18 = add i32 %17, %14
  %19 = sext i32 %18 to i64
  store i64 %19, i64* %2, align 8
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
