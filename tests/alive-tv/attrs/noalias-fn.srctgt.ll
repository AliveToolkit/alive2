define i8 @src() {
  %p = call ptr @alloc()
  store i8 3, ptr %p
  call void @xpto()
  %r = load i8, ptr %p
  ret i8 %r
}

define i8 @tgt() {
  %p = call ptr @alloc()
  store i8 3, ptr %p
  call void @xpto()
  ret i8 3
}

declare noalias ptr @alloc()
declare void @xpto()
