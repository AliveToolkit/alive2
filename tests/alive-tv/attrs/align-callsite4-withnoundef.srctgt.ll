define void @src(ptr %p) {
  call void @g(ptr %p)
  load i8, ptr %p
  ret void
}

define void @tgt(ptr %p) {
  call void @g(ptr %p)
  load i8, ptr %p, align 4
  ret void
}

declare void @g(ptr align(4) dereferenceable(4) noundef)
