@glb = external global i8

define void @src(i8 *%p) memory(argmem: readwrite) {
  call void @f(ptr %p)
  ret void
}

define void @tgt(i8 *%p) memory(argmem: readwrite) {
  unreachable
}

declare void @f(ptr)
