; TEST-ARGS: -backend-tv --disable-undef-input --disable-poison-input

define signext i8 @sext_arg_i8(i8 signext %0) {
  ret i8 %0
}