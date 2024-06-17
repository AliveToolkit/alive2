define i32 @src(ptr %f, ptr %arg) {
  call void @fn()
  call void %f(ptr %arg)
  ret i32 0
}

define i32 @tgt(ptr %f, ptr %arg) {
  call void @fn()
  call void %f(ptr %arg)
  ret i32 0
}

declare void @fn()
