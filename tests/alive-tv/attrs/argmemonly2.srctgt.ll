@x = constant i8 0
@y = constant i8 1

define void @src() {
  call void @f(ptr @x)
  call void @f(ptr @y)
  ret void
}

define void @tgt() {
  call void @f(ptr @y)
  call void @f(ptr @x)
  ret void
}

declare void @f(ptr) memory(argmem: readwrite) willreturn
