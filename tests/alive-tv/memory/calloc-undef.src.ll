define i8* @f(i8 %a) {
  %ptr = call noalias i8* @calloc(i64 1, i64 1)
  store i8 %a, i8* %ptr
  ret i8* %ptr
}

declare noalias i8* @calloc(i64, i64)
