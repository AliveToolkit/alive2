define i16 @test(i1, i16) {
  %v = select i1 %0, i16 %1, i16 undef
  ret i16 %v
}
