define { i8, i16, i8, i32 } @f() {
  %a = insertvalue { i8, i16, i8, i32 } poison, i32 1, 3
  ret { i8, i16, i8, i32 } %a
}
