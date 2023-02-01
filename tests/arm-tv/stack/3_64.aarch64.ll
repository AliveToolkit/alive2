; TEST-ARGS: --disable-undef-input --disable-poison-input

define i64 @tgt(i64 %0, i64 %1, i64 %2, i64 %3, i64 %4, i64 %5, i64 %6, i64 %7, i64 %8, i64 %9, i64 %10) local_unnamed_addr #0 {
  %x = xor i64 %10, %4
  ret i64 %x
}

attributes #0 = { mustprogress nofree norecurse nosync nounwind readnone willreturn }