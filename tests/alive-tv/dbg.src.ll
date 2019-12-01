; TEST-ARGS: -dbg

define i16 @test(i16) {
  %v = add i16 %0, %0
  ret i16 %v
}

; CHECK: num_locals: 0
