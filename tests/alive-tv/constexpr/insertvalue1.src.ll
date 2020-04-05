define { i32, i32 } @func() {
  %1 = insertvalue { i32, i32 } undef, i32 5, 0
  ret { i32, i32 } %1
}
