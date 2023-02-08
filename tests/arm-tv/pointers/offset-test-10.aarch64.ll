; ModuleID = 'foo.c'
source_filename = "foo.c"
target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128"
target triple = "arm64-apple-macosx13.0.0"

%struct.s = type { i32, i32, i32, i64, i8, i64, i32, i64 }

; Function Attrs: mustprogress nofree norecurse nosync nounwind ssp uwtable willreturn
define void @f(%struct.s* nocapture %0) local_unnamed_addr #0 {
  %2 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 2
  %3 = load i32, i32* %2, align 8
  %4 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 1
  %5 = load i32, i32* %4, align 4
  %6 = add i32 %5, %3
  %7 = trunc i32 %6 to i8
  %8 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 4
  store i8 %7, i8* %8, align 8
  %9 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 5
  %10 = load i64, i64* %9, align 8
  %11 = trunc i64 %10 to i32
  %12 = add i32 %6, %11
  %13 = sext i32 %12 to i64
  %14 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 7
  store i64 %13, i64* %14, align 8
  %15 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 0
  %16 = load i32, i32* %15, align 8
  %17 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 6
  %18 = load i32, i32* %17, align 8
  %19 = add i32 %16, %5
  %20 = add i32 %19, %12
  %21 = add i32 %20, %18
  %22 = trunc i32 %21 to i8
  store i8 %22, i8* %8, align 8
  %23 = add i32 %21, %3
  %24 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 3
  %25 = load i64, i64* %24, align 8
  %26 = trunc i64 %25 to i32
  %27 = add i32 %23, %26
  %28 = sext i32 %27 to i64
  store i64 %28, i64* %14, align 8
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
