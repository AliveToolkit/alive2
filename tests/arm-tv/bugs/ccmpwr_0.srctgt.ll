; TEST-ARGS: -backend-tv --disable-undef-input --disable-poison-input

define i1 @test18(i32 %0) {
  %2 = icmp sge i32 %0, 100
  %3 = icmp slt i32 %0, 50
  %4 = or i1 %2, %3
  ret i1 %4
}
