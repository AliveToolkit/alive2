; ModuleID = 'foo.c'
source_filename = "foo.c"
target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128"
target triple = "arm64-apple-macosx13.0.0"

%struct.s = type { i16, i16, i16, i16, i16, i32, i16, i8 }

; Function Attrs: mustprogress nofree norecurse nosync nounwind ssp uwtable willreturn
define void @f(%struct.s* nocapture %0) local_unnamed_addr #0 {
  %2 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 5
  %3 = load i32, i32* %2, align 4
  %4 = trunc i32 %3 to i16
  %5 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 6
  store i16 %4, i16* %5, align 4
  %6 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 1
  store i16 %4, i16* %6, align 2
  %7 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 3
  store i16 %4, i16* %7, align 2
  %8 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 4
  store i16 %4, i16* %8, align 4
  %9 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 2
  store i16 %4, i16* %9, align 4
  %10 = shl i32 %3, 1
  %11 = and i32 %10, 131070
  %12 = add i32 %11, %3
  %13 = trunc i32 %12 to i8
  %14 = getelementptr inbounds %struct.s, %struct.s* %0, i64 0, i32 7
  store i8 %13, i8* %14, align 2
  %15 = trunc i32 %12 to i16
  store i16 %15, i16* %9, align 4
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
