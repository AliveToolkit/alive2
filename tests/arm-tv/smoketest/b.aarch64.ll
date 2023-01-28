; TEST-ARGS: --disable-undef-input --disable-poison-input

define i5 @f(i5 %a) {
  ret i5 %a
}
