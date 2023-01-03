; TEST-ARGS: --disable-undef-input --disable-poison-input --global-isel

define i8 @tst_select_i1_i8(i1 signext %0) {
  %2 = zext i1 %0 to i8
  ret i8 %2
}