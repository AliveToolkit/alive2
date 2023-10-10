@x = global i8 0

define i8 @free_ub_glb() {
  call void @free(ptr @x)
  ret i8 0
}

declare void @free(ptr)
