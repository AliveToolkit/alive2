; TEST-ARGS: -backend-tv --disable-undef-input --disable-poison-input

define i32 @test52(i32 %x) {
  %A = shl nsw i32 %x, 3
  %B = ashr i32 %A, 1
  ret i32 %B
}