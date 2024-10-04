declare void @varargs(ptr, ...)
declare void @g(ptr, ptr)

define void @src() {
  call void (ptr, ...) @varargs(ptr null, ptr null)
  call void @g(ptr null, ptr @varargs)
  ret void
}

define void @tgt() {
  call void (ptr, ...) @varargs(ptr null, ptr null)
  call void @g(ptr null, ptr @varargs)
  ret void
}
