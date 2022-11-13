; ERROR: Source is more defined than target

@glb = external global i8

define void @src(ptr %p) memory(argmem: readwrite) {
  call void @f(ptr %p)
  ret void
}

define void @tgt(ptr %p) memory(argmem: readwrite) {
  unreachable
}

declare void @f(ptr)
