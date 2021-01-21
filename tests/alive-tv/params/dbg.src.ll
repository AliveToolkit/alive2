; TEST-ARGS: -dbg

define i16 @test(i16) {
  %v = add i16 %0, %0
  add i16 0, 0 ; dummy
  ret i16 %v
}

; CHECK: num_locals_src: 0
