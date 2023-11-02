define ptr @malloc_null() {
  %ptr = call ptr @malloc(i64 1)
  %c = icmp eq ptr %ptr, null
  br i1 %c, label %A, label %B
A:
  ret ptr null
B:
  store i8 10, ptr %ptr
  ret ptr %ptr
}

declare noalias ptr @malloc(i64)

; ERROR: Source is more defined than target
