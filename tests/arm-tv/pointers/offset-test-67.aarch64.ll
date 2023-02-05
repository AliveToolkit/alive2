; ModuleID = 'foo.c'
source_filename = "foo.c"
target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128"
target triple = "arm64-apple-macosx13.0.0"

%struct.s = type { i16, i16, i8, i8, i32, i32, i16, i16 }

; Function Attrs: mustprogress nofree norecurse nosync nounwind ssp uwtable willreturn
define void @f(%struct.s* nocapture %0) local_unnamed_addr #0 {
  %2 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 0
  %3 = load i16, i16* %2, align 4
  %4 = sext i16 %3 to i32
  %5 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 4
  %6 = load i32, i32* %5, align 4
  %7 = add nsw i32 %6, %4
  %8 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 5
  store i32 %7, i32* %8, align 4
  %9 = trunc i32 %7 to i16
  %10 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 7
  store i16 %9, i16* %10, align 2
  %11 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 1
  %12 = load i16, i16* %11, align 2
  %13 = zext i16 %12 to i32
  %14 = add nsw i32 %7, %13
  store i32 %14, i32* %5, align 4
  %15 = trunc i32 %14 to i16
  store i16 %15, i16* %11, align 2
  %16 = and i32 %7, 65535
  %17 = add nsw i32 %14, %16
  %18 = trunc i32 %17 to i16
  store i16 %18, i16* %2, align 4
  %19 = trunc i32 %17 to i8
  %20 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 2
  store i8 %19, i8* %20, align 4
  store i16 %18, i16* %10, align 2
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
