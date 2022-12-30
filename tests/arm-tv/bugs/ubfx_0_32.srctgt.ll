; TEST-ARGS: -backend-tv --disable-undef-input --disable-poison-input

define i32 @test51(i32 %x) {
  %A = shl nuw i32 %x, 1
  %B = lshr i32 %A, 3
  ret i32 %B
}