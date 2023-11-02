define i32 @free_ub_alloca() {
  %1 = alloca i32
  call void @free(ptr %1)
  ret i32 0
}

declare void @free(ptr)
