; TEST-ARGS: -backend-tv --disable-undef-input --disable-poison-input

define i32 @src(i32 %x) {
  %A = shl i32 %x, 3
  %B = lshr i32 %A, 1
  ret i32 %B
}