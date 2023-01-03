; TEST-ARGS: --disable-undef-input --disable-poison-input

define signext i48 @test6(i64 %0) {
  %2 = lshr i64 %0, 30
  %3 = trunc i64 %2 to i48
  ret i48 %3
}