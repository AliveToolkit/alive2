@x = global i8 0

define i8 @free_ub_glb() {
  call void @free(i8* @x)
  ret i8 1
}

declare void @free(i8*)
