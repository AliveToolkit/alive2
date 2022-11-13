@x = constant i8 0

define void @src() {
  call void @f(ptr @x)
  ret void
}

define void @tgt() memory(none) {
  call void @f(ptr @x)
  ret void
}

declare void @f(ptr) memory(argmem: readwrite)
