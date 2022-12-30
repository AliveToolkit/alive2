; TEST-ARGS: -backend-tv --disable-undef-input --disable-poison-input

define signext i32 @test6(i64 %0) {
  %2 = lshr i64 %0, 30
  %3 = trunc i64 %2 to i32
  ret i32 %3
}