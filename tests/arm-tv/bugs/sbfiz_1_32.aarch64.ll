; TEST-ARGS:

define i32 @test52(i32 %x) {
  %B = shl nsw i32 %x, 2
  ret i32 %B
}