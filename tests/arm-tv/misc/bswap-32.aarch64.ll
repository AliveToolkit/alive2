; ModuleID = 'bswap.ll'
source_filename = "bswap.c"
target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128"
target triple = "arm64-apple-macosx13.0.0"

; Function Attrs: noinline nounwind ssp uwtable(sync)
define i32 @byte_swap_32(i32 noundef %0) #0 {
  %.sroa.0.0.extract.trunc = trunc i32 %0 to i8
  %.sroa.2.0.extract.shift = lshr i32 %0, 8
  %.sroa.2.0.extract.trunc = trunc i32 %.sroa.2.0.extract.shift to i8
  %.sroa.3.0.extract.shift = lshr i32 %0, 16
  %.sroa.3.0.extract.trunc = trunc i32 %.sroa.3.0.extract.shift to i8
  %.sroa.4.0.extract.shift = lshr i32 %0, 24
  %.sroa.4.0.extract.trunc = trunc i32 %.sroa.4.0.extract.shift to i8
  %2 = zext i8 %.sroa.0.0.extract.trunc to i32
  %3 = shl i32 %2, 24
  %4 = zext i8 %.sroa.2.0.extract.trunc to i32
  %5 = shl i32 %4, 16
  %6 = or i32 %3, %5
  %7 = zext i8 %.sroa.3.0.extract.trunc to i32
  %8 = shl i32 %7, 8
  %9 = or i32 %6, %8
  %10 = zext i8 %.sroa.4.0.extract.trunc to i32
  %11 = shl i32 %10, 0
  %12 = or i32 %9, %11
  ret i32 %12
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
