; ModuleID = 'bswap.ll'
source_filename = "bswap.c"
target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128"
target triple = "arm64-apple-macosx13.0.0"

; Function Attrs: noinline nounwind ssp uwtable(sync)
define i64 @byte_swap_64(i64 noundef %0) #0 {
  %.sroa.0.0.extract.trunc = trunc i64 %0 to i8
  %.sroa.2.0.extract.shift = lshr i64 %0, 8
  %.sroa.2.0.extract.trunc = trunc i64 %.sroa.2.0.extract.shift to i8
  %.sroa.3.0.extract.shift = lshr i64 %0, 16
  %.sroa.3.0.extract.trunc = trunc i64 %.sroa.3.0.extract.shift to i8
  %.sroa.4.0.extract.shift = lshr i64 %0, 24
  %.sroa.4.0.extract.trunc = trunc i64 %.sroa.4.0.extract.shift to i8
  %.sroa.5.0.extract.shift = lshr i64 %0, 32
  %.sroa.5.0.extract.trunc = trunc i64 %.sroa.5.0.extract.shift to i8
  %.sroa.6.0.extract.shift = lshr i64 %0, 40
  %.sroa.6.0.extract.trunc = trunc i64 %.sroa.6.0.extract.shift to i8
  %.sroa.7.0.extract.shift = lshr i64 %0, 48
  %.sroa.7.0.extract.trunc = trunc i64 %.sroa.7.0.extract.shift to i8
  %.sroa.8.0.extract.shift = lshr i64 %0, 56
  %.sroa.8.0.extract.trunc = trunc i64 %.sroa.8.0.extract.shift to i8
  %2 = sext i8 %.sroa.0.0.extract.trunc to i64
  %3 = shl i64 %2, 56
  %4 = sext i8 %.sroa.2.0.extract.trunc to i64
  %5 = shl i64 %4, 48
  %6 = or i64 %3, %5
  %7 = sext i8 %.sroa.3.0.extract.trunc to i64
  %8 = shl i64 %7, 40
  %9 = or i64 %6, %8
  %10 = sext i8 %.sroa.4.0.extract.trunc to i64
  %11 = shl i64 %10, 32
  %12 = or i64 %9, %11
  %13 = sext i8 %.sroa.5.0.extract.trunc to i64
  %14 = shl i64 %13, 24
  %15 = or i64 %12, %14
  %16 = sext i8 %.sroa.6.0.extract.trunc to i64
  %17 = shl i64 %16, 16
  %18 = or i64 %15, %17
  %19 = sext i8 %.sroa.7.0.extract.trunc to i64
  %20 = shl i64 %19, 8
  %21 = or i64 %18, %20
  %22 = sext i8 %.sroa.8.0.extract.trunc to i64
  %23 = shl i64 %22, 0
  %24 = or i64 %21, %23
  ret i64 %24
}

attributes #0 = { noinline nounwind ssp uwtable(sync) "frame-pointer"="non-leaf" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="apple-m1" "target-features"="+aes,+crc,+crypto,+dotprod,+fp-armv8,+fp16fml,+fullfp16,+lse,+neon,+ras,+rcpc,+rdm,+sha2,+sha3,+sm4,+v8.1a,+v8.2a,+v8.3a,+v8.4a,+v8.5a,+v8a,+zcm,+zcz" }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 2, !"SDK Version", [2 x i32] [i32 14, i32 0]}
!1 = !{i32 1, !"wchar_size", i32 4}
!2 = !{i32 8, !"PIC Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 1}
!4 = !{i32 7, !"frame-pointer", i32 1}
!5 = !{!"clang version 16.0.5"}
