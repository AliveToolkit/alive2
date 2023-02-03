; TEST-ARGS:

define i64 @test51(i64 %x) {
  %A = shl nuw i64 %x, 1
  %B = lshr i64 %A, 3
  ret i64 %B
}