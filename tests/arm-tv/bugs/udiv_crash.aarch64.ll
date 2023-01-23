; TEST-ARGS: --disable-undef-input --disable-poison-input --global-isel

define noundef signext i32 @f(i1 %0, i64 signext %1) {
  %3 = zext i1 %0 to i32
  %4 = udiv i32 %3, 134087678
  ret i32 %4
}
