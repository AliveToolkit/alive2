define { i8, i32 } @f({ ptr, i32 } %x) {
  %ex = extractvalue { ptr, i32 } %x, 1
  %ins = insertvalue { i8, i32 } undef, i32 %ex, 1
  ret { i8, i32 } %ins
}
