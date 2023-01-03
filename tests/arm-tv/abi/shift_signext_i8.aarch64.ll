; TEST-ARGS: --disable-undef-input --disable-poison-input

define signext i8 @test6(i64 %0) {
  %2 = lshr i64 %0, 30
  %3 = trunc i64 %2 to i8
  ret i8 %3
}