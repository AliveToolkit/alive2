; TEST-ARGS: -backend-tv --disable-undef-input --disable-poison-input

define signext i1 @not_i1(i1 signext %0) {
  %2 = xor i1 %0, true
  ret i1 %2
}