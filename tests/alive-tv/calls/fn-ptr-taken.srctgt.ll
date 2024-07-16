declare void @varargs(ptr, ...)
declare void @g(ptr, ptr)

define void @fn() {
  call void (ptr, ...) @varargs(ptr null, ptr null)
  call void @g(ptr null, ptr @varargs)
  ret void
}
