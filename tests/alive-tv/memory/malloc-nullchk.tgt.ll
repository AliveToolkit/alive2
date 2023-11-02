define ptr @malloc_null() {
  %ptr = call ptr @malloc(i64 1)
  store i8 10, ptr %ptr
  ret ptr %ptr
}

declare noalias ptr @malloc(i64)
