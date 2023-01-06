; TEST-ARGS: --disable-undef-input --disable-poison-input

define signext i16 @test7(i64 %0) {
  %2 = lshr i64 %0, 23
  %3 = trunc i64 %2 to i16
  ret i16 %3
}
