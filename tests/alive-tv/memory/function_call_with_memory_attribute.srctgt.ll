declare void @use_writeonly(ptr) memory(write)

define void @src() {
  %src = alloca i32, align 4
  %dest = alloca i32, align 4
  call void @use_writeonly(ptr %dest)
  ret void
}

define void @tgt() {
  %src = alloca i32, align 4
  %dest = alloca i32, align 4
  call void @use_writeonly(ptr %dest)
  ret void
}
