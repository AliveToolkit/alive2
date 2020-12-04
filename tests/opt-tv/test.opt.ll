; TEST-ARGS: -instcombine

define i32 @test(i32 %x) {
  %v = add i32 %x, 1
  %w = sub i32 %v, 1
  ret i32 %w
}
