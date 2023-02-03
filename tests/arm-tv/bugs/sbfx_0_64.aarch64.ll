; TEST-ARGS:

define i64 @test50(i64 %x) {
  %A = shl nsw i64 %x, 1
  %B = ashr i64 %A, 3
  ret i64 %B
}