; TEST-ARGS: -backend-tv --disable-undef-input --disable-poison-input

define i8 @test() {
  ret i8 undef
}
