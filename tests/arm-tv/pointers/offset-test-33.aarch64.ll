; ModuleID = 'foo.c'
source_filename = "foo.c"
target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128"
target triple = "arm64-apple-macosx13.0.0"

%struct.s = type { i32, i64, i8, i32, i8, i8, i8, i16 }

; Function Attrs: mustprogress nofree norecurse nosync nounwind ssp uwtable willreturn
define void @f(%struct.s* nocapture %0) local_unnamed_addr #0 {
  %2 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 5
  store i8 0, i8* %2, align 1
  %3 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 4
  store i8 0, i8* %3, align 8
  %4 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 1
  %5 = load i64, i64* %4, align 8
  %6 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 2
  %7 = load i8, i8* %6, align 8
  %8 = zext i8 %7 to i64
  %9 = shl i64 %5, 1
  %10 = add i64 %9, %8
  %11 = trunc i64 %10 to i32
  %12 = trunc i64 %10 to i8
  store i8 %12, i8* %6, align 8
  %13 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 3
  %14 = load i32, i32* %13, align 4
  %15 = add i32 %14, %11
  %16 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 7
  %17 = load i16, i16* %16, align 4
  %18 = zext i16 %17 to i32
  %19 = add i32 %15, %18
  %20 = trunc i32 %19 to i8
  store i8 %20, i8* %6, align 8
  %21 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 6
  %22 = load i8, i8* %21, align 2
  %23 = zext i8 %22 to i32
  %24 = add i32 %19, %23
  %25 = trunc i32 %24 to i16
  store i16 %25, i16* %16, align 4
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
