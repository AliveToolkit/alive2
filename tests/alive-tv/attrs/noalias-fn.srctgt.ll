define i8 @src() {
  %p = call i8* @alloc()
  store i8 3, i8* %p
  call void @xpto()
  %r = load i8, i8* %p
  ret i8 %r
}

define i8 @tgt() {
  %p = call i8* @alloc()
  store i8 3, i8* %p
  call void @xpto()
  ret i8 3
}

declare noalias i8* @alloc()
declare void @xpto()
