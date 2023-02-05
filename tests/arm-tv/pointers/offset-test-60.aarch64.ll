; ModuleID = 'foo.c'
source_filename = "foo.c"
target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128"
target triple = "arm64-apple-macosx13.0.0"

%struct.s = type { i8, i16, i64, i16, i8, i32, i8, i32 }

; Function Attrs: mustprogress nofree norecurse nosync nounwind ssp uwtable willreturn
define void @f(%struct.s* nocapture %0) local_unnamed_addr #0 {
  %2 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 6
  %3 = load i8, i8* %2, align 8
  %4 = zext i8 %3 to i32
  %5 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 4
  %6 = load i8, i8* %5, align 2
  %7 = zext i8 %6 to i32
  %8 = add nuw nsw i32 %7, %4
  %9 = trunc i32 %8 to i8
  %10 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 0
  store i8 %9, i8* %10, align 8
  %11 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 2
  %12 = load i64, i64* %11, align 8
  %13 = trunc i64 %12 to i32
  %14 = add i32 %8, %13
  %15 = trunc i32 %14 to i8
  store i8 %15, i8* %2, align 8
  %16 = and i32 %8, 255
  %17 = add nsw i32 %14, %16
  %18 = trunc i32 %17 to i8
  store i8 %18, i8* %10, align 8
  %19 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 5
  %20 = load i32, i32* %19, align 4
  %21 = add nsw i32 %17, %20
  %22 = trunc i32 %21 to i8
  store i8 %22, i8* %10, align 8
  %23 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 1
  %24 = load i16, i16* %23, align 2
  %25 = zext i16 %24 to i32
  %26 = add nsw i32 %21, %25
  %27 = trunc i32 %26 to i8
  store i8 %27, i8* %10, align 8
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
