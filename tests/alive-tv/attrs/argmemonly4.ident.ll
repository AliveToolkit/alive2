define void @fn(ptr %p, ptr %q) {
  call void @f(ptr %p)
  call void @f(ptr %q) memory(argmem: readwrite)
  ret void
}

declare void @f(ptr)
