; TEST-ARGS: --disable-undef-input --disable-poison-input

define i32 @tgt(i32 %0, i32 %1, i32 %2, i32 %3, i32 %4, i32 %5, i32 %6, i32 %7, i32 %8, i32 %9, i32 %10) local_unnamed_addr #0 {
  %x = xor i32 %10, %4
  ret i32 %x
}

attributes #0 = { mustprogress nofree norecurse nosync nounwind readnone willreturn }