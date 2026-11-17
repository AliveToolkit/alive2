; ERROR: Value mismatch

define i32 @src(<2 x i32> %v) {
  %w = insertelement <2 x i32> %v, i32 7, i1 1
  %e = extractelement <2 x i32> %w, i64 1
  ret i32 %e
}

define i32 @tgt(<2 x i32> %v) {
  ret i32 8
}
