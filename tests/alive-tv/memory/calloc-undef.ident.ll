define ptr @f(i8 %a) {
  %ptr = call noalias ptr @calloc(i64 1, i64 1)
  store i8 %a, ptr %ptr
  ret ptr %ptr
}

declare noalias ptr @calloc(i64, i64)
