define { i32, i32 } @func() {
  %1 = insertvalue { i32, i32 } {i32 1, i32 2}, i32 5, 0
  %2 = insertvalue { i32, i32 } %1, i32 10, 1
  ret { i32, i32 } %2
}

; ERROR: Value mismatch