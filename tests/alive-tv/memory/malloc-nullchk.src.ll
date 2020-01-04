define i8* @malloc_null() {
  %ptr = call noalias i8* @malloc(i64 1)
  %c = icmp eq i8* %ptr, null
  br i1 %c, label %A, label %B
A:
  ret i8* null
B:
  store i8 10, i8* %ptr
  ret i8* %ptr
}

declare noalias i8* @malloc(i64)

; ERROR: Source is more defined than target
