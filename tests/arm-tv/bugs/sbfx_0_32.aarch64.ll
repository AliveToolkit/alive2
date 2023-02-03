; TEST-ARGS:

define i32 @test50(i32 %x) {
  %A = shl nsw i32 %x, 1
  %B = ashr i32 %A, 3
  ret i32 %B
}