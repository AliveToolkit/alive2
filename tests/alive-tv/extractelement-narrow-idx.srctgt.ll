; ERROR: Value mismatch

define i32 @src(<4 x i32> %v) {
  %e = extractelement <4 x i32> %v, i2 1
  ret i32 %e
}

define i32 @tgt(<4 x i32> %v) {
  ret i32 42
}
